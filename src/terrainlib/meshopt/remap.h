#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <ranges>
#include <span>
#include <type_traits>
#include <vector>

#include <glm/glm.hpp>
#include <libassert/assert.hpp>
#include <meshoptimizer.h>

#include "glm_utils.h"

namespace meshopt {

// The index a remap gives a vertex it drops.
inline constexpr uint32_t DROPPED_VERTEX = ~0u;

// Where each vertex of a buffer moves.
struct [[nodiscard]] VertexRemap {
    std::vector<uint32_t> table; // new index of each vertex, or DROPPED_VERTEX
    size_t remapped_vertex_count = 0;
};

// Maps vertices whose bytes match in every stream onto one index, considering only the vertices `indices` uses unless it is empty.
// The streams must be equally long, and their padding bytes zero.
template <std::ranges::contiguous_range... Streams>
[[nodiscard]] inline VertexRemap generate_vertex_remap(const std::span<const uint32_t> indices, const Streams &...streams) {
    static_assert(sizeof...(Streams) >= 1 && sizeof...(Streams) <= 16, "meshopt remaps over 1 to 16 streams");
    static_assert((std::is_trivially_copyable_v<std::ranges::range_value_t<Streams>> && ...), "Vertex types must be trivially copyable");
    static_assert(((sizeof(std::ranges::range_value_t<Streams>) <= 256) && ...), "meshopt compares at most 256 bytes per vertex and stream");

    const std::array<size_t, sizeof...(Streams)> stream_sizes{std::ranges::size(streams)...};
    const size_t vertex_count = stream_sizes.front();
    ASSERT(std::ranges::all_of(stream_sizes, [vertex_count](const size_t size) { return size == vertex_count; }));
    DEBUG_ASSERT(std::ranges::all_of(indices, [vertex_count](const uint32_t index) { return index < vertex_count; }));

    const std::array<meshopt_Stream, sizeof...(Streams)> stream_descriptions{meshopt_Stream{
        std::ranges::data(streams),
        sizeof(std::ranges::range_value_t<Streams>),
        sizeof(std::ranges::range_value_t<Streams>)}...};

    // meshopt reads a missing index buffer as one index per vertex.
    const uint32_t *index_data = indices.empty() ? nullptr : indices.data();
    const size_t index_count = indices.empty() ? vertex_count : indices.size();

    VertexRemap remap;
    remap.table.resize(vertex_count);
    remap.remapped_vertex_count = meshopt_generateVertexRemapMulti(
        remap.table.data(),
        index_data,
        index_count,
        vertex_count,
        stream_descriptions.data(),
        stream_descriptions.size());
    return remap;
}

// Reorders vertices for locality of reference in `indices`, dropping the vertices it never uses.
[[nodiscard]] inline VertexRemap optimize_vertex_fetch_remap(const std::span<const uint32_t> indices, const size_t vertex_count) {
    DEBUG_ASSERT(std::ranges::all_of(indices, [vertex_count](const uint32_t index) { return index < vertex_count; }));

    VertexRemap remap;
    remap.table.resize(vertex_count);
    remap.remapped_vertex_count = meshopt_optimizeVertexFetchRemap(
        remap.table.data(),
        indices.data(),
        indices.size(),
        vertex_count);
    return remap;
}

// Moves every vertex `remap` keeps to its new index.
template <std::ranges::contiguous_range Vertices>
[[nodiscard]] inline std::vector<std::ranges::range_value_t<Vertices>> remap_vertex_buffer(const Vertices &vertices, const VertexRemap &remap) {
    using Vertex = std::ranges::range_value_t<Vertices>;
    static_assert(std::is_trivially_copyable_v<Vertex>, "Vertex type must be trivially copyable");
    static_assert(sizeof(Vertex) <= 256, "meshopt remaps vertices of at most 256 bytes");

    const size_t vertex_count = std::ranges::size(vertices);
    ASSERT(remap.table.size() == vertex_count);

    std::vector<Vertex> remapped(remap.remapped_vertex_count);
    meshopt_remapVertexBuffer(
        remapped.data(),
        std::ranges::data(vertices),
        vertex_count,
        sizeof(Vertex),
        remap.table.data());
    return remapped;
}

namespace detail {

// Writes the new index of each of `indices` into `destination`, which must be as long; no index may refer to a dropped vertex.
inline void remap_index_buffer(const std::span<uint32_t> destination, const std::span<const uint32_t> indices, const VertexRemap &remap) {
    ASSERT(destination.size() == indices.size());
    DEBUG_ASSERT(std::ranges::all_of(indices, [&remap](const uint32_t index) {
        return index < remap.table.size() && remap.table[index] != DROPPED_VERTEX;
    }));

    meshopt_remapIndexBuffer(
        destination.data(),
        indices.data(),
        indices.size(),
        remap.table.data());
}

} // namespace detail

// Rewrites every index to its vertex's new index; no index may refer to a dropped vertex.
[[nodiscard]] inline std::vector<uint32_t> remap_index_buffer(const std::span<const uint32_t> indices, const VertexRemap &remap) {
    std::vector<uint32_t> remapped(indices.size());
    detail::remap_index_buffer(remapped, indices, remap);
    return remapped;
}
[[nodiscard]] inline std::vector<glm::uvec3> remap_index_buffer(const std::span<const glm::uvec3> triangles, const VertexRemap &remap) {
    std::vector<glm::uvec3> remapped(triangles.size());
    detail::remap_index_buffer(flatten(remapped), flatten(triangles), remap);
    return remapped;
}

} // namespace meshopt
