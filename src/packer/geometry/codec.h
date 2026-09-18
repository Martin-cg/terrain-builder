#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <ranges>
#include <span>
#include <utility>
#include <vector>

#include <glm/glm.hpp>
#include <libassert/assert.hpp>
#include <tl/expected.hpp>

#include "containers/OffsetTable.h"
#include "enumerate.h"
#include "format.h"
#include "meshopt/encode.h"
#include "range_utils.h"
#include "try.h"

namespace packer::geometry {

namespace detail {
    inline constexpr int VERTEX_CODEC_LEVEL = 2;
    inline constexpr int VERTEX_CODEC_VERSION = 1;

}

// Encodes `page` into an encoded geometry page.
[[nodiscard]] inline EncodedPage encode(const Page &page) {
    const OffsetTable vertex_offsets = build_offset_table(page.clusters | std::views::transform(&ClusterHeader::vertex_count));
    const OffsetTable triangle_offsets = build_offset_table(page.clusters | std::views::transform(&ClusterHeader::triangle_count));
    ASSERT(std::in_range<decltype(EncodedPage::position_count)>(page.positions.size()));
    ASSERT(page.positions.size() <= vertex_offsets.total_size());
    ASSERT(vertex_offsets.total_size() == page.position_indices.size());
    ASSERT(vertex_offsets.total_size() == page.uvs.size());
    ASSERT(triangle_offsets.total_size() == page.triangles.size());

    EncodedPage encoded;
    encoded.position_count = static_cast<uint32_t>(page.positions.size());
    encoded.positions = meshopt::encode_vertex_buffer(std::span(page.positions), detail::VERTEX_CODEC_LEVEL, detail::VERTEX_CODEC_VERSION);
    encoded.uvs = meshopt::encode_vertex_buffer(std::span(page.uvs), detail::VERTEX_CODEC_LEVEL, detail::VERTEX_CODEC_VERSION);

    for (const auto &[cluster_index, cluster] : enumerate(page.clusters)) {
        const std::span<const uint32_t> position_indices = std::span(page.position_indices).subspan(
            vertex_offsets.get_begin(cluster_index),
            vertex_offsets.segment_size(cluster_index));
        const std::span<const glm::u8vec3> triangles = std::span(page.triangles).subspan(
            triangle_offsets.get_begin(cluster_index),
            triangle_offsets.segment_size(cluster_index));
        const std::vector<uint8_t> meshlet = meshopt::encode_meshlet(position_indices, triangles);
        ASSERT(meshlet.size() <= std::numeric_limits<decltype(EncodedClusterHeader::encoded_meshlet_byte_count)>::max());

        encoded.clusters.push_back(EncodedClusterHeader{
            .vertex_count = cluster.vertex_count,
            .triangle_count = cluster.triangle_count,
            .encoded_meshlet_byte_count = static_cast<uint16_t>(meshlet.size()),
        });
        encoded.meshlets.append_range(meshlet);
    }

    return encoded;
}

// Decodes `encoded` into a geometry page.
[[nodiscard]] inline tl::expected<Page, meshopt::DecodeError> decode(const EncodedPage &encoded) {
    const OffsetTable meshlet_offsets = build_offset_table(encoded.clusters | std::views::transform(&EncodedClusterHeader::encoded_meshlet_byte_count));
    const size_t page_vertex_count = sum(encoded.clusters, size_t{0}, &EncodedClusterHeader::vertex_count);
    const size_t page_triangle_count = sum(encoded.clusters, size_t{0}, &EncodedClusterHeader::triangle_count);

    TRY(std::vector<QuantizedPoint> positions, meshopt::try_decode_vertex_buffer<QuantizedPoint>(encoded.position_count, encoded.positions));
    TRY(std::vector<QuantizedUv> uvs, meshopt::try_decode_vertex_buffer<QuantizedUv>(page_vertex_count, encoded.uvs));

    std::vector<uint32_t> position_indices;
    std::vector<glm::u8vec3> triangles;
    position_indices.reserve(page_vertex_count);
    triangles.reserve(page_triangle_count);
    for (const auto &[cluster_index, cluster] : enumerate(encoded.clusters)) {
        DEBUG_ASSERT(meshlet_offsets.get_end(cluster_index) <= encoded.meshlets.size());
        const std::span<const uint8_t> meshlet = std::span(encoded.meshlets).subspan(
            meshlet_offsets.get_begin(cluster_index),
            meshlet_offsets.segment_size(cluster_index));
        TRY(meshopt::try_decode_meshlet(meshlet, cluster.vertex_count, cluster.triangle_count, position_indices, triangles));
    }

    std::vector<ClusterHeader> clusters = transform_vector(encoded.clusters, [](const EncodedClusterHeader &c) {
        return ClusterHeader{c.vertex_count, c.triangle_count};
    });

    Page page{
        .clusters = std::move(clusters),
        .positions = std::move(positions),
        .position_indices = std::move(position_indices),
        .uvs = std::move(uvs),
        .triangles = std::move(triangles),
    };
    return page;
}

}