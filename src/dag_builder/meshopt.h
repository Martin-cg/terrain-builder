#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include <libassert/assert.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <meshoptimizer.h>
#include <tl/expected.hpp>

#include "glm_utils.h"
#include "numeric/int_math.h"
#include "range_utils.h"

namespace meshopt {

inline constexpr size_t NO_TARGET_COUNT = 0;
inline constexpr float NO_TARGET_ERROR = FLT_MAX;

struct [[nodiscard]] SimplifyResult {
    std::vector<glm::uvec3> triangles;
    float relative_error = 0.0f;
};

inline SimplifyResult simplify(
    const std::span<const glm::uvec3> triangles,
    const std::span<const glm::vec3> positions,
    const size_t target_triangle_count,
    const float target_error,
    const uint32_t options = 0) {
    const size_t vertex_count = positions.size();
    const size_t triangle_count = triangles.size();
    const size_t index_count = triangle_count * 3;
    const size_t positions_stride = sizeof(glm::vec3);
    const size_t target_index_count = target_triangle_count * 3;

    SimplifyResult result;
    result.triangles.resize(triangle_count);

    // Flatten spans
    const std::span<const uint32_t> indices = flatten(triangles);
    const std::span<const float> positions_flat = flatten(positions);
    const std::span<uint32_t> result_indices = flatten(result.triangles);

    const size_t new_index_count = meshopt_simplify(
        result_indices.data(),
        indices.data(),
        index_count,
        positions_flat.data(),
        vertex_count,
        positions_stride,
        target_index_count,
        target_error,
        options,
        &result.relative_error);

    // Resize to match returned number of indices
    const size_t new_triangle_count = new_index_count / 3;
    result.triangles.resize(new_triangle_count);

    return result;
}

inline SimplifyResult simplify_with_attributes(
    const std::span<const glm::uvec3> triangles,
    const std::span<const glm::vec3> positions,
    const std::span<const float> vertex_attributes,
    const size_t vertex_attribute_stride,
    const std::span<const float> vertex_attribute_weights,
    const std::span<const uint8_t> vertex_locks,
    const size_t target_triangle_count,
    const float target_error,
    const uint32_t options) {
    const size_t vertex_count = positions.size();
    const size_t triangle_count = triangles.size();
    const size_t index_count = triangle_count * 3;
    const size_t attribute_count = vertex_attribute_weights.size();
    const size_t positions_stride = sizeof(glm::vec3);
    const size_t target_index_count = target_triangle_count * 3;

    DEBUG_ASSERT(vertex_attributes.size() >= vertex_count * (vertex_attribute_stride / sizeof(float)));
    DEBUG_ASSERT(vertex_locks.size() == vertex_count || vertex_locks.empty());

    SimplifyResult result;
    result.triangles.resize(triangle_count);

    // Flatten spans
    const std::span<const uint32_t> indices = flatten(triangles);
    const std::span<const float> positions_flat = flatten(positions);
    const std::span<uint32_t> result_indices = flatten(result.triangles);
    const uint8_t* vertex_lock_ptr = vertex_locks.empty() ? nullptr : vertex_locks.data();

    const size_t new_index_count = meshopt_simplifyWithAttributes(
        result_indices.data(),
        indices.data(),
        index_count,
        positions_flat.data(),
        vertex_count,
        positions_stride,
        vertex_attributes.data(),
        vertex_attribute_stride,
        vertex_attribute_weights.data(),
        attribute_count,
        vertex_lock_ptr,
        target_index_count,
        target_error,
        options,
        &result.relative_error);

    // Resize to match returned number of indices
    const size_t new_triangle_count = new_index_count / 3;
    result.triangles.resize(new_triangle_count);

    return result;
}

struct [[nodiscard]] BuildMeshletsResult {
    std::vector<meshopt_Meshlet> meshlets;
    std::vector<uint32_t> vertex_indices;  // flat vertex indices for all meshlets
    std::vector<glm::tvec3<uint8_t>> local_triangles; // flat triangle indices for all meshlets
};

inline BuildMeshletsResult build_meshlets(
    const std::span<const glm::uvec3> triangles,
    const std::span<const glm::vec3> positions,
    const uint32_t max_vertices,
    const uint32_t min_triangles,
    const uint32_t max_triangles,
    const float cone_weight = 0.5f,
    const float split_factor = 0.0f) {
    ASSERT(max_vertices <= 256, "Max vertices per cluster must be less than 256.");

    // Flatten inputs
    const std::span<const uint32_t> indices = flatten(triangles);
    const std::span<const float> positions_flat = flatten(positions);

    // Compute maximum number of meshlets needed
    const size_t index_count = indices.size();
    const uint32_t max_meshlets = meshopt_buildMeshletsBound(index_count, max_vertices, min_triangles);

    // Allocate buffers
    std::vector<meshopt_Meshlet> meshlets(max_meshlets);
    std::vector<uint32_t> meshlet_vertices(index_count);
    std::vector<glm::tvec3<uint8_t>> meshlet_triangles(triangles.size());
    std::span<uint8_t> meshlet_indices = flatten(meshlet_triangles);

    // Build meshlets
    const size_t meshlet_count = meshopt_buildMeshletsFlex(
        meshlets.data(),
        meshlet_vertices.data(),
        meshlet_indices.data(),
        indices.data(),
        index_count,
        positions_flat.data(),
        positions.size(),
        sizeof(glm::vec3),
        max_vertices,
        min_triangles,
        max_triangles,
        cone_weight,
        split_factor);
    ASSERT(meshlet_count <= max_meshlets);

    // Trim meshlet output buffer
    meshlets.resize(meshlet_count);

    // Convert index offsets into triangle offsets
    for (meshopt_Meshlet& meshlet : meshlets) {
        DEBUG_ASSERT(meshlet.triangle_offset % 3 == 0);
        meshlet.triangle_offset /= 3;
    }

    // Trim unused vertex and triangle buffers
    if (meshlet_count > 0) {
        const meshopt_Meshlet &last = meshlets[meshlet_count - 1];
        const size_t full_vertex_count = last.vertex_offset + last.vertex_count;
        const size_t full_triangle_count = last.triangle_offset + last.triangle_count;
        ASSERT(meshlet_vertices.size() >= full_vertex_count);
        ASSERT(meshlet_triangles.size() >= full_triangle_count);
        meshlet_vertices.resize(full_vertex_count);
        meshlet_triangles.resize(full_triangle_count);
    }

    return BuildMeshletsResult{
        std::move(meshlets),
        std::move(meshlet_vertices),
        std::move(meshlet_triangles)};
}

inline void optimize_meshlet(
    const std::span<uint32_t> vertex_indices,
    const std::span<glm::tvec3<uint8_t>> triangles) {
    const std::span<uint8_t> indices = flatten(triangles);

    meshopt_optimizeMeshlet(
        vertex_indices.data(),
        indices.data(),
        triangles.size(),
        vertex_indices.size());
}

struct [[nodiscard]] PartitionClustersResult {
    std::vector<uint32_t> cluster_partitions;
    uint32_t partition_count = 0;
};

inline PartitionClustersResult partition_clusters(
    const std::span<const uint32_t> cluster_indices,       // flattened vertex indices of all clusters
    const std::span<const uint32_t> cluster_vertex_counts, // number of vertices per cluster
    const std::span<const glm::vec3> vertex_positions,
    const uint32_t clusters_per_partition) {
    ASSERT(clusters_per_partition > 0, "clusters_per_partition must be > 0");

    // Allocate cluster to partition map
    const size_t cluster_count = cluster_vertex_counts.size();
    std::vector<uint32_t> partitions(cluster_count);

    const size_t total_index_count = sum(cluster_vertex_counts, size_t{0});
    const std::span<const float> positions_flat = flatten(vertex_positions);

    const size_t partition_count = meshopt_partitionClusters(
        partitions.data(),
        cluster_indices.data(),
        total_index_count,
        cluster_vertex_counts.data(),
        cluster_count,
        positions_flat.data(),
        vertex_positions.size(),
        sizeof(glm::vec3),
        clusters_per_partition
    );

    return PartitionClustersResult{std::move(partitions), static_cast<uint32_t>(partition_count)};
}

// A decoder's failure, carrying the code meshopt returned.
struct DecodeError {
    int code = 0;

    auto operator<=>(const DecodeError &) const = default;
};

// The level `meshopt_encodeVertexBuffer` encodes at.
inline constexpr int DEFAULT_VERTEX_ENCODE_LEVEL = 2;
// Defers to the version set through `meshopt_encodeVertexVersion`.
inline constexpr int DEFAULT_VERTEX_ENCODE_VERSION = -1;

template <typename T>
inline std::vector<uint8_t> encode_vertex_buffer(
    const std::span<const T> data,
    const int level = DEFAULT_VERTEX_ENCODE_LEVEL,
    const int version = DEFAULT_VERTEX_ENCODE_VERSION) {
    static_assert(std::is_trivially_copyable_v<T>, "Vertex type must be trivially copyable");

    std::vector<uint8_t> buf(meshopt_encodeVertexBufferBound(data.size(), sizeof(T)));
    const size_t written = meshopt_encodeVertexBufferLevel(buf.data(), buf.size(), data.data(), data.size(), sizeof(T), level, version);
    ASSERT(written != 0);

    buf.resize(written);
    return buf;
}
template <typename T>
inline std::vector<uint8_t> encode_vertex_buffer(const std::vector<T>& data) {
    return encode_vertex_buffer(std::span<const T>(data));
}

template <typename T>
[[nodiscard]] inline tl::expected<void, DecodeError> try_decode_vertex_buffer(std::span<T> data, const std::span<const uint8_t> encoded) {
    static_assert(std::is_trivially_copyable_v<T>, "Vertex type must be trivially copyable");

    const int code = meshopt_decodeVertexBuffer(data.data(), data.size(), sizeof(T), encoded.data(), encoded.size());
    if (code != 0) {
        return tl::unexpected(DecodeError{code});
    }
    return {};
}
template <typename T>
inline void decode_vertex_buffer(std::span<T> data, const std::span<const uint8_t> encoded) {
    const tl::expected<void, DecodeError> result = try_decode_vertex_buffer(data, encoded);
    ASSERT(result.has_value(), "Vertex buffer decoding failed");
}
template <typename T>
inline void decode_vertex_buffer(std::vector<T>& data, size_t vertex_count, const std::span<const uint8_t> encoded) {
    data.resize(vertex_count);
    decode_vertex_buffer(std::span<T>(data), encoded);
}
template <typename T>
inline std::vector<T> decode_vertex_buffer(size_t vertex_count, const std::span<const uint8_t> encoded) {
    std::vector<T> data;
    decode_vertex_buffer(data, vertex_count, encoded);
    return data;
}

inline std::vector<uint8_t> encode_index_buffer(const std::span<const uint32_t> indices) {
    const size_t index_count = indices.size();
    const size_t buffer_bound = meshopt_encodeIndexBufferBound(index_count, sizeof(uint32_t));
    std::vector<uint8_t> buf(buffer_bound);
    const size_t written = meshopt_encodeIndexBuffer(buf.data(), buf.size(), indices.data(), index_count);
    DEBUG_ASSERT(written <= buf.size());
    buf.resize(written);
    return buf;
}
inline std::vector<uint8_t> encode_index_buffer(const std::span<const glm::uvec3> triangles) {
    std::span<const uint32_t> indices = flatten(triangles);
    return encode_index_buffer(indices);
}

inline void decode_index_buffer(std::span<uint32_t> indices, const std::span<const uint8_t> encoded) {
    const int result = meshopt_decodeIndexBuffer(indices.data(), indices.size(), sizeof(uint32_t), encoded.data(), encoded.size());
    ASSERT(result == 0, "Index buffer decoding failed");
}
inline void decode_index_buffer(std::span<glm::uvec3> triangles, const std::span<const uint8_t> encoded) {
    std::span<uint32_t> indices = flatten(triangles);
    decode_index_buffer(indices, encoded);
}
inline void decode_index_buffer(std::vector<glm::uvec3>& triangles, const size_t triangle_count, const std::span<const uint8_t> encoded) {
    triangles.resize(triangle_count);
    decode_index_buffer(std::span<glm::uvec3>(triangles), encoded);
}
inline std::vector<glm::uvec3> decode_index_buffer(const size_t triangle_count, const std::span<const uint8_t> encoded) {
    std::vector<glm::uvec3> triangles;
    decode_index_buffer(triangles, triangle_count, encoded);
    return triangles;
}

// Encodes one meshlet: its references into a shared vertex buffer, and its triangles as indices into those references.
inline std::vector<uint8_t> encode_meshlet(const std::span<const uint32_t> vertices, const std::span<const glm::tvec3<uint8_t>> triangles) {
    const std::span<const uint8_t> indices = flatten(triangles);

    std::vector<uint8_t> buf(meshopt_encodeMeshletBound(vertices.size(), triangles.size()));
    const size_t written = meshopt_encodeMeshlet(buf.data(), buf.size(), vertices.data(), vertices.size(), indices.data(), triangles.size());
    ASSERT(written != 0);

    buf.resize(written);
    return buf;
}

namespace detail {

// The number of references the meshlet decoder may write when asked for `vertex_count` of them.
[[nodiscard]] inline size_t get_meshlet_decode_vertex_bound(const size_t vertex_count) {
    // The decoder writes whole 4-byte blocks.
    return align_up(vertex_count * sizeof(uint16_t), size_t{4}) / sizeof(uint16_t);
}

// The number of triangles the meshlet decoder may write when asked for `triangle_count` of them.
[[nodiscard]] inline size_t get_meshlet_decode_triangle_bound(const size_t triangle_count) {
    // The decoder writes whole 4-byte blocks, which need not end on a whole triangle.
    return int_div_ceil(align_up(triangle_count * sizeof(glm::tvec3<uint8_t>), size_t{4}), sizeof(glm::tvec3<uint8_t>));
}

// Decodes a meshlet into the front of `vertices` and `triangles`, which must hold at least the decode bounds.
[[nodiscard]] inline tl::expected<void, DecodeError> try_decode_meshlet(
    const std::span<const uint8_t> encoded,
    const size_t vertex_count,
    const size_t triangle_count,
    const std::span<uint16_t> vertices,
    const std::span<glm::tvec3<uint8_t>> triangles) {
    ASSERT(vertices.size() >= get_meshlet_decode_vertex_bound(vertex_count));
    ASSERT(triangles.size() >= get_meshlet_decode_triangle_bound(triangle_count));

    const int code = meshopt_decodeMeshlet(
        vertices.data(),
        vertex_count,
        sizeof(uint16_t),
        triangles.data(),
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

// Appends the meshlet's references to `vertices` and its triangles to `triangles`; on failure both keep their previous contents.
[[nodiscard]] inline tl::expected<void, DecodeError> try_decode_meshlet(
    const std::span<const uint8_t> encoded,
    const size_t vertex_count,
    const size_t triangle_count,
    std::vector<uint16_t> &vertices,
    std::vector<glm::tvec3<uint8_t>> &triangles) {
    const size_t vertex_offset = vertices.size();
    const size_t triangle_offset = triangles.size();
    vertices.resize(vertex_offset + detail::get_meshlet_decode_vertex_bound(vertex_count));
    triangles.resize(triangle_offset + detail::get_meshlet_decode_triangle_bound(triangle_count));

    const tl::expected<void, DecodeError> result = detail::try_decode_meshlet(
        encoded,
        vertex_count,
        triangle_count,
        std::span(vertices).subspan(vertex_offset),
        std::span(triangles).subspan(triangle_offset));
    if (!result) {
        vertices.resize(vertex_offset);
        triangles.resize(triangle_offset);
        return result;
    }

    vertices.resize(vertex_offset + vertex_count);
    triangles.resize(triangle_offset + triangle_count);
    return result;
}

} // namespace meshopt
