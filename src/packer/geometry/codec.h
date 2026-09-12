#pragma once

#include <algorithm>
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

#include "PageError.h"
#include "cluster.h"
#include "clusterize.h"
#include "containers/OffsetTable.h"
#include "enumerate.h"
#include "format.h"
#include "glm_utils.h"
#include "meshopt/encode.h"
#include "range_utils.h"
#include "try.h"

namespace packer::geometry {

namespace detail {
    inline constexpr int VERTEX_CODEC_LEVEL = 2;
    inline constexpr int VERTEX_CODEC_VERSION = 1;

    [[nodiscard]] inline bool check_counts(const uint32_t vertex_count, const uint32_t triangle_count) {
        return vertex_count <= ClusterOptions::MAX_VERTEX_LIMIT && triangle_count <= MAX_TRIANGLES_PER_CLUSTER;
    }

    // Whether every triangle index lies within its cluster's vertices and every position index within `page.positions`.
    [[nodiscard]] inline bool check_indices(const Page &page) {
        const size_t position_count = page.positions.size();
        const bool position_indices_are_valid = std::ranges::all_of(page.position_indices, [position_count](const uint32_t index) {
            return index < position_count;
        });
        if (!position_indices_are_valid) {
            return false;
        }

        const OffsetTable triangle_offsets = build_offset_table(page.clusters | std::views::transform(&ClusterHeader::triangle_count));
        for (const auto &[cluster_index, cluster] : enumerate(page.clusters)) {
            const std::span<const glm::u8vec3> triangles = std::span(page.triangles).subspan(
                triangle_offsets.get_begin(cluster_index),
                triangle_offsets.segment_size(cluster_index));
            const uint32_t vertex_count = cluster.vertex_count;
            const bool triangles_are_valid = std::ranges::all_of(flatten(triangles), [vertex_count](const uint8_t index) {
                return index < vertex_count;
            });
            if (!triangles_are_valid) {
                return false;
            }
        }
        return true;
    }

}

// Encodes every stream of `page`; `decode` reverses it.
[[nodiscard]] inline EncodedPage encode(const Page &page) {
    const OffsetTable vertex_offsets = build_offset_table(page.clusters | std::views::transform(&ClusterHeader::vertex_count));
    const OffsetTable triangle_offsets = build_offset_table(page.clusters | std::views::transform(&ClusterHeader::triangle_count));
    ASSERT(std::in_range<decltype(EncodedPage::position_count)>(page.positions.size()));
    ASSERT(page.positions.size() <= vertex_offsets.total_size());
    ASSERT(vertex_offsets.total_size() == page.position_indices.size());
    ASSERT(vertex_offsets.total_size() == page.uvs.size());
    ASSERT(triangle_offsets.total_size() == page.triangles.size());
    DEBUG_ASSERT(detail::check_indices(page));

    EncodedPage encoded;
    encoded.position_count = static_cast<uint32_t>(page.positions.size());
    encoded.positions = meshopt::encode_vertex_buffer(std::span(page.positions), detail::VERTEX_CODEC_LEVEL, detail::VERTEX_CODEC_VERSION);
    encoded.uvs = meshopt::encode_vertex_buffer(std::span(page.uvs), detail::VERTEX_CODEC_LEVEL, detail::VERTEX_CODEC_VERSION);

    for (const auto &[cluster_index, cluster] : enumerate(page.clusters)) {
        ASSERT(detail::check_counts(cluster.vertex_count, cluster.triangle_count));

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

// Decodes the page `encoded` stands for, failing when a stream does not decode or the streams disagree.
[[nodiscard]] inline tl::expected<Page, PageError> decode(const EncodedPage &encoded) {
    // The counts size every allocation below and must stay within meshopt's limits, and a malformed page can carry anything.
    const bool counts_are_valid = std::ranges::all_of(encoded.clusters, [](const EncodedClusterHeader &c) {
        return detail::check_counts(c.vertex_count, c.triangle_count);
    });
    if (!counts_are_valid) {
        return tl::unexpected(PageMalformed{});
    }

    const OffsetTable meshlet_offsets = build_offset_table(encoded.clusters | std::views::transform(&EncodedClusterHeader::encoded_meshlet_byte_count));
    if (meshlet_offsets.total_size() != encoded.meshlets.size()) {
        return tl::unexpected(PageMalformed{});
    }

    const size_t page_vertex_count = sum(encoded.clusters, size_t{0}, &EncodedClusterHeader::vertex_count);
    const size_t page_triangle_count = sum(encoded.clusters, size_t{0}, &EncodedClusterHeader::triangle_count);

    // Every position is referenced by a cluster vertex, which also bounds the allocation below.
    if (encoded.position_count > page_vertex_count) {
        return tl::unexpected(PageMalformed{});
    }
    TRY(std::vector<QuantizedPoint> positions, meshopt::try_decode_vertex_buffer<QuantizedPoint>(encoded.position_count, encoded.positions));
    TRY(std::vector<QuantizedUv> uvs, meshopt::try_decode_vertex_buffer<QuantizedUv>(page_vertex_count, encoded.uvs));

    std::vector<uint32_t> position_indices;
    std::vector<glm::u8vec3> triangles;
    position_indices.reserve(page_vertex_count);
    triangles.reserve(page_triangle_count);
    for (const auto &[cluster_index, cluster] : enumerate(encoded.clusters)) {
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
    if (!detail::check_indices(page)) {
        return tl::unexpected(PageMalformed{});
    }
    return page;
}

}