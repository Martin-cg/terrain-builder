#pragma once

#include <cstdint>
#include <span>
#include <utility>
#include <vector>

#include <glm/glm.hpp>
#include <libassert/assert.hpp>

#include "cluster.h"
#include "codec.h"
#include "containers/OffsetTable.h"
#include "containers/SegmentedBuffer.h"
#include "format.h"
#include "meshopt/filter.h"
#include "meshopt/meshlets.h"
#include "meshopt/remap.h"
#include "quantize.h"
#include "range_utils.h"

namespace packer::geometry {

namespace detail {

    // Every vertex of a page's clusters, cluster by cluster, with its position on the global grid.
    struct PageVertices {
        std::vector<QuantizedPoint> positions;
        std::vector<QuantizedUv> uvs;
        OffsetTable_<uint32_t> cluster_offsets; // vertex range of each cluster, in page order
    };

    // Quantizes every vertex of every cluster of `clusterings`, cluster by cluster, onto the global grid.
    [[nodiscard]] inline PageVertices quantize_vertices(const std::span<const Clustering> clusterings) {
        // TODO: additional meshopt-style quantization for compression.
        PageVertices vertices;
        for (const Clustering &clustering : clusterings) {
            for (const Cluster &cluster : clustering.clusters) {
                vertices.cluster_offsets.append_segment(cluster.vertex_count());
                for (const uint32_t index : cluster.vertex_indices) {
                    const glm::dvec3 &position = clustering.positions[index];
                    vertices.positions.push_back(quantize_point(position));
                }
                for (const glm::dvec2 &uv : cluster.uvs) {
                    vertices.uvs.push_back(quantize_uv(uv));
                }
            }
        }
        ASSERT(std::in_range<uint32_t>(vertices.positions.size()));
        return vertices;
    }

    // The distinct positions, and for each input position the index of its match among them.
    struct DeduplicatePositionsResult {
        std::vector<QuantizedPoint> positions;
        std::vector<uint32_t> position_ids;
    };

    // Stores each distinct position of `positions` once.
    [[nodiscard]] inline DeduplicatePositionsResult deduplicate_positions(const std::span<const QuantizedPoint> positions) {
        meshopt::VertexRemap remap = meshopt::generate_vertex_remap({}, positions);
        std::vector<QuantizedPoint> unique_positions = meshopt::remap_vertex_buffer(positions, remap);
        return DeduplicatePositionsResult{
            .positions = std::move(unique_positions),
            .position_ids = std::move(remap.table),
        };
    }

    inline constexpr int MESHLET_OPTIMIZE_LEVEL = 3;

    // Every cluster's meshlet, one segment per cluster.
    struct PageMeshlets {
        SegmentedBuffer<uint32_t, uint32_t> vertex_indices; // into the page vertices
        SegmentedBuffer<glm::u8vec3, uint32_t> local_triangles; // into the cluster's vertex_indices
    };

    // Builds each cluster's meshlet over `vertices`, dropping degenerate and duplicate triangles; `position_ids` must hold one id per vertex.
    [[nodiscard]] inline PageMeshlets build_meshlets(
        const std::span<const Clustering> clusterings,
        const PageVertices &vertices,
        const std::span<const uint32_t> position_ids) {
        ASSERT(position_ids.size() == vertices.positions.size());

        PageMeshlets meshlets;
        uint32_t cluster_index = 0;
        for (const Clustering &clustering : clusterings) {
            for (const Cluster &cluster : clustering.clusters) {
                const uint32_t vertex_offset = vertices.cluster_offsets.get_begin(cluster_index);
                const std::span<const uint32_t> cluster_position_ids = position_ids.subspan(vertex_offset, vertices.cluster_offsets.segment_size(cluster_index));
                const std::vector<glm::uvec3> kept_triangles = meshopt::filter_index_buffer(cluster.local_triangles, cluster_position_ids);

                meshopt::extract_meshlet_indices(kept_triangles, meshlets.vertex_indices, meshlets.local_triangles);
                const uint32_t vertex_count = meshlets.vertex_indices.segment_size(cluster_index);
                const uint32_t triangle_count = meshlets.local_triangles.segment_size(cluster_index);
                ASSERT(check_counts(vertex_count, triangle_count));

                for (uint32_t &vertex_index : meshlets.vertex_indices.last_segment()) {
                    vertex_index += vertex_offset;
                }
                meshopt::optimize_meshlet(meshlets.vertex_indices.last_segment(), meshlets.local_triangles.last_segment(), MESHLET_OPTIMIZE_LEVEL);
                cluster_index++;
            }
        }
        return meshlets;
    }

    // Every meshlet vertex's UV and position id.
    struct GatherVerticesResult {
        std::vector<QuantizedUv> uvs;
        std::vector<uint32_t> position_ids;
    };

    // Gives every meshlet vertex its UV and position id, in the order of `vertex_indices`.
    [[nodiscard]] inline GatherVerticesResult gather_vertices(
        const std::span<const uint32_t> vertex_indices,
        const std::span<const QuantizedUv> uvs,
        const std::span<const uint32_t> position_ids) {
        std::vector<QuantizedUv> gathered_uvs = transform_vector(vertex_indices, [&](const uint32_t i) { return uvs[i]; });
        std::vector<uint32_t> gathered_position_ids = transform_vector(vertex_indices, [&](const uint32_t i) { return position_ids[i]; });
        return GatherVerticesResult{
            .uvs = std::move(gathered_uvs),
            .position_ids = std::move(gathered_position_ids),
        };
    }

    // The positions reordered for locality of reference, and each vertex's index into them.
    struct OrderPositionsResult {
        std::vector<QuantizedPoint> positions;
        std::vector<uint32_t> position_indices;
    };

    // Reorders `positions` for locality of reference in `position_ids`, dropping the positions no id refers to.
    [[nodiscard]] inline OrderPositionsResult order_positions(
        const std::span<const QuantizedPoint> positions,
        const std::span<const uint32_t> position_ids) {
        const meshopt::VertexRemap remap = meshopt::optimize_vertex_fetch_remap(position_ids, positions.size());
        std::vector<QuantizedPoint> ordered_positions = meshopt::remap_vertex_buffer(positions, remap);
        std::vector<uint32_t> position_indices = meshopt::remap_index_buffer(position_ids, remap);
        return OrderPositionsResult{
            .positions = std::move(ordered_positions),
            .position_indices = std::move(position_indices),
        };
    }

    // The header of every cluster in `meshlets`.
    [[nodiscard]] inline std::vector<ClusterHeader> get_cluster_headers(const PageMeshlets &meshlets) {
        std::vector<ClusterHeader> clusters;
        for (uint32_t i = 0; i < meshlets.vertex_indices.segment_count(); i++) {
            clusters.push_back(ClusterHeader{
                .vertex_count = meshlets.vertex_indices.segment_size(i),
                .triangle_count = meshlets.local_triangles.segment_size(i),
            });
        }
        return clusters;
    }

}

// Packs every cluster of `clusterings`, in order, into one page, where a cluster may end up with no triangles.
// Every cluster must have one UV per vertex.
[[nodiscard]] inline Page build(const std::span<const Clustering> clusterings) {
    const detail::PageVertices vertices = detail::quantize_vertices(clusterings);
    const detail::DeduplicatePositionsResult deduplicated = detail::deduplicate_positions(vertices.positions);
    detail::PageMeshlets meshlets = detail::build_meshlets(clusterings, vertices, deduplicated.position_ids);
    detail::GatherVerticesResult gathered = detail::gather_vertices(meshlets.vertex_indices.flat(), vertices.uvs, deduplicated.position_ids);
    detail::OrderPositionsResult ordered = detail::order_positions(deduplicated.positions, gathered.position_ids);
    return Page{
        .clusters = detail::get_cluster_headers(meshlets),
        .positions = std::move(ordered.positions),
        .position_indices = std::move(ordered.position_indices),
        .uvs = std::move(gathered.uvs),
        .triangles = std::move(meshlets.local_triangles.backing()),
    };
}

}