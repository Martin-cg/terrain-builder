#include <algorithm>
#include <cstdint>
#include <span>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtx/hash.hpp>
#include <libassert/assert.hpp>

#include "build_config.h"
#include "containers/FixedVector.h"
#include "containers/HybridVector.h"
#include "containers/SegmentedBuffer.h"
#include "enumerate.h"
#include "mesh/connectivity/adjacency.h"
#include "mesh/connectivity/edges.h"
#include "mesh/connectivity/manifold.h"
#include "mesh/connectivity/vertex_index_range.h"
#include "vector_utils.h"

namespace mesh {

namespace {
template <typename IndexContainer>
std::unordered_map<glm::uvec2, IndexContainer>
create_edge_to_triangle_mapping_impl(const std::span<const glm::uvec3> triangles) {
    std::unordered_map<glm::uvec2, IndexContainer> edges_to_triangles;

    for_each_halfedge(triangles, [&](const glm::uvec2 &edge, const uint32_t triangle_index) {
        edges_to_triangles[edge].push_back(triangle_index);
    }, true);

    if constexpr (IS_DEBUG_BUILD) {
        // Validate the mapping
        for (const auto &[edge, triangle_indices] : edges_to_triangles) {
            DEBUG_ASSERT(!triangle_indices.empty(), "Each edge must have at least one triangle");

            for (const uint32_t triangle_index : triangle_indices) {
                DEBUG_ASSERT(triangle_index < triangles.size(), "Triangle index must be valid");

                const glm::uvec3 &triangle = triangles[triangle_index];

                const bool first_vertex_found_in_triangle =
                    (triangle[0] == edge.x || triangle[1] == edge.x || triangle[2] == edge.x);

                const bool second_vertex_found_in_triangle =
                    (triangle[0] == edge.y || triangle[1] == edge.y || triangle[2] == edge.y);

                DEBUG_ASSERT(first_vertex_found_in_triangle && second_vertex_found_in_triangle, "Both vertices of the edge exist in the triangle");
                ALP_UNUSED(first_vertex_found_in_triangle);
                ALP_UNUSED(second_vertex_found_in_triangle);
            }
        }
    }

    return edges_to_triangles;
}
} // namespace

std::unordered_map<glm::uvec2, FixedVector<uint32_t, 2>> create_edge_to_triangle_mapping_manifold(const std::span<const glm::uvec3> triangles) {
    return create_edge_to_triangle_mapping_impl<FixedVector<uint32_t, 2>>(triangles);
}
std::unordered_map<glm::uvec2, HybridVector<uint32_t, 2>> create_edge_to_triangle_mapping_non_manifold(const std::span<const glm::uvec3> triangles) {
    return create_edge_to_triangle_mapping_impl<HybridVector<uint32_t, 2>>(triangles);
}
std::unordered_map<glm::uvec2, std::vector<uint32_t>> create_edge_to_triangle_mapping_non_manifold2(const std::span<const glm::uvec3> triangles) {
    return create_edge_to_triangle_mapping_impl<std::vector<uint32_t>>(triangles);
}

namespace {
struct QueriedEdge {
    uint32_t low;
    uint32_t high;
    uint32_t slot;

    auto operator<=>(const QueriedEdge &) const = default;
};
} // namespace

SegmentedBuffer<uint32_t, uint32_t> find_edge_neighbourhoods(const std::span<const glm::uvec3> triangles, const std::span<const uint32_t> queried) {
    std::vector<QueriedEdge> queried_edges;
    queried_edges.reserve(3 * queried.size());
    for (const auto [slot, triangle_index] : enumerate<uint32_t>(queried)) {
        for_each_halfedge(triangles.subspan(triangle_index, 1), [&](const glm::uvec2 edge) {
            queried_edges.push_back({edge.x, edge.y, slot});
        }, true);
    }
    std::ranges::sort(queried_edges);

    const auto project_edge = [](const QueriedEdge &queried_edge) {
        return std::pair(queried_edge.low, queried_edge.high);
    };

    std::vector<std::pair<uint32_t, uint32_t>> pairs;
    std::vector<uint32_t> slots;
    for (uint32_t triangle_index = 0; triangle_index < triangles.size(); triangle_index++) {
        slots.clear();
        for_each_halfedge(triangles.subspan(triangle_index, 1), [&](const glm::uvec2 edge) {
            for (const QueriedEdge &hit : std::ranges::equal_range(queried_edges, std::pair(edge.x, edge.y), {}, project_edge)) {
                slots.push_back(hit.slot);
            }
        }, true);

        // A copy of a queried triangle shares all three of its edges.
        dedup_by_sort(slots);
        for (const uint32_t slot : slots) {
            pairs.emplace_back(slot, triangle_index);
        }
    }

    return group_by_key<uint32_t, uint32_t>(static_cast<uint32_t>(queried.size()), pairs);
}

std::vector<std::vector<uint32_t>> create_vertex_to_triangle_mapping(const std::span<const glm::uvec3> triangles) {
    return create_vertex_to_triangle_mapping(triangles, vertex_buffer_size(triangles));
}
std::vector<std::vector<uint32_t>> create_vertex_to_triangle_mapping(const std::span<const glm::uvec3> triangles, const uint32_t vertex_count) {
    DEBUG_ASSERT(vertex_count >= vertex_buffer_size(triangles));

    std::vector<std::vector<uint32_t>> vertex_to_triangles(vertex_count);

    for (uint32_t triangle_index = 0; triangle_index < triangles.size(); triangle_index++) {
        const glm::uvec3 &triangle = triangles[triangle_index];

        vertex_to_triangles[triangle.x].push_back(triangle_index);
        vertex_to_triangles[triangle.y].push_back(triangle_index);
        vertex_to_triangles[triangle.z].push_back(triangle_index);
    }

    return vertex_to_triangles;
}

std::vector<uint32_t> count_vertex_adjacent_triangles(const std::span<const glm::uvec3> triangles) {
    return count_vertex_adjacent_triangles(triangles, vertex_buffer_size(triangles));
}
std::vector<uint32_t> count_vertex_adjacent_triangles(const std::span<const glm::uvec3> triangles, const uint32_t vertex_count) {
    DEBUG_ASSERT(vertex_count >= vertex_buffer_size(triangles));

    std::vector<uint32_t> adjacent_triangle_count(vertex_count, 0);

    for (const glm::uvec3 &triangle : triangles) {
        for (uint8_t k = 0; k < 3; k++) {
            adjacent_triangle_count[triangle[k]]++;
        }
    }

    return adjacent_triangle_count;
}

std::vector<uint32_t> find_unused_vertices(const std::span<const glm::uvec3> triangles) {
    return find_unused_vertices(triangles, vertex_buffer_size(triangles));
}
std::vector<uint32_t> find_unused_vertices(const std::span<const glm::uvec3> triangles, const uint32_t vertex_count) {
    DEBUG_ASSERT(vertex_count >= vertex_buffer_size(triangles));

    std::vector<bool> connected;
    connected.resize(vertex_count, false);
    for (const glm::uvec3 &triangle : triangles) {
        for (uint8_t k = 0; k < 3; k++) {
            connected[triangle[k]] = true;
        }
    }

    std::vector<uint32_t> unused;
    for (uint32_t i = 0; i < vertex_count; i++) {
        if (!connected[i]) {
            unused.push_back(i);
        }
    }

    return unused;
}

std::vector<std::vector<uint32_t>> build_vertex_adjacency(const std::span<const glm::uvec3> triangles) {
    return build_vertex_adjacency(triangles, vertex_buffer_size(triangles));
}
std::vector<std::vector<uint32_t>> build_vertex_adjacency(const std::span<const glm::uvec3> triangles, const uint32_t vertex_count) {
    DEBUG_ASSERT(vertex_count >= vertex_buffer_size(triangles));
    std::vector<std::vector<uint32_t>> adjacency(vertex_count);

    auto add_edge = [&](const uint32_t a, const uint32_t b) {
        adjacency[a].push_back(b);
        adjacency[b].push_back(a);
    };

    for (const glm::uvec3 &triangle : triangles) {
        add_edge(triangle.x, triangle.y);
        add_edge(triangle.y, triangle.z);
        add_edge(triangle.z, triangle.x);
    }

    for (auto &neighbours : adjacency) {
        dedup_by_sort(neighbours);
    }

    return adjacency;
}

bool is_consistently_oriented(const std::span<const glm::uvec3> triangles) {
    if (!is_edge_manifold(triangles)) {
        return false;
    }

    std::unordered_set<glm::uvec2> observed;
    bool duplicate_found = false;
    for_each_halfedge(triangles, [&](const glm::uvec2 &edge) {
        const auto [_, inserted] = observed.emplace(edge);
        if (!inserted) {
            duplicate_found = true;
            return false;
        }
        return true;
    });
    return !duplicate_found;
}

}
