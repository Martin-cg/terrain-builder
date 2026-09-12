#pragma once

#include <algorithm>
#include <cstdint>
#include <span>
#include <type_traits>
#include <vector>

#include <glm/glm.hpp>
#include <libassert/assert.hpp>
#include <meshoptimizer.h>
#include <tl/expected.hpp>

#include "glm_utils.h"
#include "numeric/int_math.h"

namespace meshopt {

// A decoder's failure, carrying the code meshopt returned.
struct DecodeError {
    int code = 0;

    auto operator<=>(const DecodeError &) const = default;
};

// The level `meshopt_encodeVertexBuffer` encodes at.
inline constexpr int DEFAULT_VERTEX_ENCODE_LEVEL = 2;
// Defers to the version set through `meshopt_encodeVertexVersion`.
inline constexpr int DEFAULT_VERTEX_ENCODE_VERSION = -1;

// Encodes a vertex buffer with meshopt's vertex codec.
template <typename T>
[[nodiscard]] inline std::vector<uint8_t> encode_vertex_buffer(
    const std::span<const T> data,
    const int level = DEFAULT_VERTEX_ENCODE_LEVEL,
    const int version = DEFAULT_VERTEX_ENCODE_VERSION) {
    static_assert(std::is_trivially_copyable_v<T>, "Vertex type must be trivially copyable");

    const size_t bound = meshopt_encodeVertexBufferBound(data.size(), sizeof(T));
    std::vector<uint8_t> buf(bound);
    const size_t written = meshopt_encodeVertexBufferLevel(
        buf.data(),
        buf.size(),
        data.data(),
        data.size(),
        sizeof(T),
        level,
        version);
    ASSERT(written != 0);

    buf.resize(written);
    return buf;
}
template <typename T>
[[nodiscard]] inline std::vector<uint8_t> encode_vertex_buffer(const std::vector<T>& data) {
    return encode_vertex_buffer(std::span<const T>(data));
}

// Decodes a vertex buffer into `data`, which must hold exactly as many vertices as were encoded.
template <typename T>
[[nodiscard]] inline tl::expected<void, DecodeError> try_decode_vertex_buffer(std::span<T> data, const std::span<const uint8_t> encoded) {
    static_assert(std::is_trivially_copyable_v<T>, "Vertex type must be trivially copyable");

    const int code = meshopt_decodeVertexBuffer(
        data.data(),
        data.size(),
        sizeof(T),
        encoded.data(),
        encoded.size());
    if (code != 0) {
        return tl::unexpected(DecodeError{code});
    }
    return {};
}
// Decodes a vertex buffer into `data`, which must hold exactly as many vertices as were encoded; asserts that it decodes.
template <typename T>
inline void decode_vertex_buffer(std::span<T> data, const std::span<const uint8_t> encoded) {
    const tl::expected<void, DecodeError> result = try_decode_vertex_buffer(data, encoded);
    ASSERT(result.has_value(), "Vertex buffer decoding failed");
}
// Replaces the contents of `data` with the decoded vertices.
template <typename T>
inline void decode_vertex_buffer(std::vector<T>& data, size_t vertex_count, const std::span<const uint8_t> encoded) {
    data.resize(vertex_count);
    decode_vertex_buffer(std::span<T>(data), encoded);
}
// Decodes a vertex buffer into a new vector.
template <typename T>
[[nodiscard]] inline std::vector<T> decode_vertex_buffer(size_t vertex_count, const std::span<const uint8_t> encoded) {
    std::vector<T> data;
    decode_vertex_buffer(data, vertex_count, encoded);
    return data;
}

// Encodes a triangle list with meshopt's index codec.
[[nodiscard]] inline std::vector<uint8_t> encode_index_buffer(const std::span<const uint32_t> indices) {
    const size_t index_count = indices.size();
    const size_t vertex_count = indices.empty() ? 0 : size_t{std::ranges::max(indices)} + 1;
    const size_t bound = meshopt_encodeIndexBufferBound(index_count, vertex_count);
    std::vector<uint8_t> buf(bound);
    const size_t written = meshopt_encodeIndexBuffer(buf.data(), buf.size(), indices.data(), index_count);
    ASSERT(written != 0);
    buf.resize(written);
    return buf;
}
[[nodiscard]] inline std::vector<uint8_t> encode_index_buffer(const std::span<const glm::uvec3> triangles) {
    std::span<const uint32_t> indices = flatten(triangles);
    return encode_index_buffer(indices);
}

// Decodes a triangle list into `indices`, which must hold exactly as many indices as were encoded; asserts that it decodes.
inline void decode_index_buffer(std::span<uint32_t> indices, const std::span<const uint8_t> encoded) {
    const int result = meshopt_decodeIndexBuffer(
        indices.data(),
        indices.size(),
        sizeof(uint32_t),
        encoded.data(),
        encoded.size());
    ASSERT(result == 0, "Index buffer decoding failed");
}
// Decodes a triangle list into `triangles`, which must hold exactly as many triangles as were encoded; asserts that it decodes.
inline void decode_index_buffer(std::span<glm::uvec3> triangles, const std::span<const uint8_t> encoded) {
    std::span<uint32_t> indices = flatten(triangles);
    decode_index_buffer(indices, encoded);
}
// Replaces the contents of `triangles` with the decoded triangles.
inline void decode_index_buffer(std::vector<glm::uvec3>& triangles, const size_t triangle_count, const std::span<const uint8_t> encoded) {
    triangles.resize(triangle_count);
    decode_index_buffer(std::span<glm::uvec3>(triangles), encoded);
}
// Decodes a triangle list into a new vector.
[[nodiscard]] inline std::vector<glm::uvec3> decode_index_buffer(const size_t triangle_count, const std::span<const uint8_t> encoded) {
    std::vector<glm::uvec3> triangles;
    decode_index_buffer(triangles, triangle_count, encoded);
    return triangles;
}

// Encodes one meshlet with meshopt's meshlet codec.
[[nodiscard]] inline std::vector<uint8_t> encode_meshlet(
    const std::span<const uint32_t> vertex_indices,
    const std::span<const glm::tvec3<uint8_t>> local_triangles) {
    const std::span<const uint8_t> local_indices = flatten(local_triangles);

    const size_t bound = meshopt_encodeMeshletBound(vertex_indices.size(), local_triangles.size());
    std::vector<uint8_t> buf(bound);
    const size_t written = meshopt_encodeMeshlet(
        buf.data(),
        buf.size(),
        vertex_indices.data(),
        vertex_indices.size(),
        local_indices.data(),
        local_triangles.size());
    ASSERT(written != 0);

    buf.resize(written);
    return buf;
}

namespace detail {

// The number of vertex indices the meshlet decoder may write when asked for `vertex_count` of them.
[[nodiscard]] inline size_t get_meshlet_decode_vertex_bound(const size_t vertex_count) {
    // The decoder writes whole 4-byte blocks.
    const size_t byte_bound = align_up(vertex_count * sizeof(uint16_t), size_t{4});
    return byte_bound / sizeof(uint16_t);
}

// The number of local triangles the meshlet decoder may write when asked for `triangle_count` of them.
[[nodiscard]] inline size_t get_meshlet_decode_triangle_bound(const size_t triangle_count) {
    // The decoder writes whole 4-byte blocks, which need not end on a whole triangle.
    const size_t byte_bound = align_up(triangle_count * sizeof(glm::tvec3<uint8_t>), size_t{4});
    return int_div_ceil(byte_bound, sizeof(glm::tvec3<uint8_t>));
}

// Decodes a meshlet into the front of `vertex_indices` and `local_triangles`, which must hold at least the decode bounds.
[[nodiscard]] inline tl::expected<void, DecodeError> try_decode_meshlet(
    const std::span<const uint8_t> encoded,
    const size_t vertex_count,
    const size_t triangle_count,
    const std::span<uint16_t> vertex_indices,
    const std::span<glm::tvec3<uint8_t>> local_triangles) {
    ASSERT(vertex_indices.size() >= get_meshlet_decode_vertex_bound(vertex_count));
    ASSERT(local_triangles.size() >= get_meshlet_decode_triangle_bound(triangle_count));

    const int code = meshopt_decodeMeshlet(
        vertex_indices.data(),
        vertex_count,
        sizeof(uint16_t),
        local_triangles.data(),
        triangle_count,
        sizeof(glm::tvec3<uint8_t>),
        encoded.data(),
        encoded.size());
    if (code != 0) {
        return tl::unexpected(DecodeError{code});
    }
    return {};
}

} // namespace detail

// Appends the decoded meshlet to `vertex_indices` and `local_triangles`; on failure both keep their previous contents.
[[nodiscard]] inline tl::expected<void, DecodeError> try_decode_meshlet(
    const std::span<const uint8_t> encoded,
    const size_t vertex_count,
    const size_t triangle_count,
    std::vector<uint16_t> &vertex_indices,
    std::vector<glm::tvec3<uint8_t>> &local_triangles) {
    const size_t vertex_offset = vertex_indices.size();
    const size_t triangle_offset = local_triangles.size();
    const size_t vertex_bound = detail::get_meshlet_decode_vertex_bound(vertex_count);
    const size_t triangle_bound = detail::get_meshlet_decode_triangle_bound(triangle_count);
    vertex_indices.resize(vertex_offset + vertex_bound);
    local_triangles.resize(triangle_offset + triangle_bound);

    const tl::expected<void, DecodeError> result = detail::try_decode_meshlet(
        encoded,
        vertex_count,
        triangle_count,
        std::span(vertex_indices).subspan(vertex_offset),
        std::span(local_triangles).subspan(triangle_offset));
    if (!result) {
        vertex_indices.resize(vertex_offset);
        local_triangles.resize(triangle_offset);
        return result;
    }

    vertex_indices.resize(vertex_offset + vertex_count);
    local_triangles.resize(triangle_offset + triangle_count);
    return result;
}

} // namespace meshopt
