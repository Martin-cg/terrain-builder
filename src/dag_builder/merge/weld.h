#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <ranges>
#include <span>
#include <stdexcept>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>
#include <libassert/assert.hpp>

#include "containers/UnionFind.h"
#include "VecHash.h"
#include "cluster.h"
#include "compact.h"
#include "enumerate.h"
#include "log.h"
#include "mesh/connectivity/adjacency.h"
#include "spatial_lookup/Hashmap.h"
#include "validate.h"

struct WeldingMap {
    std::vector<uint32_t> position_to_welded;
    uint32_t welded_position_count = 0;
};

namespace detail {

// Throws std::invalid_argument when epsilon is negative or not finite.
inline void validate_epsilon(const double epsilon) {
    if (!(epsilon >= 0.0) || !std::isfinite(epsilon)) {
        throw std::invalid_argument("epsilon must be finite and non negative");
    }
}

// Maps each position onto the welded position of its transitive epsilon neighbourhood.
template <std::ranges::forward_range Positions>
    requires std::ranges::sized_range<Positions>
WeldingMap build_epsilon_welding_map(Positions &&positions, const double epsilon) {
    const uint32_t position_count = std::ranges::size(positions);

    spatial_lookup::Hashmap3d<uint32_t> spatial_map(epsilon * 5);
    for (const auto &[position_index, position] : enumerate<uint32_t>(positions)) {
        spatial_map.insert(position, position_index);
    }

    UnionFind_<false, uint32_t, uint32_t> neighbourhoods(position_count);
    std::vector<uint32_t> matches;

    for (const auto &[position_index, position] : enumerate<uint32_t>(positions)) {
        // Find all positions within epsilon.
        matches.clear();
        spatial_map.find_all_near(position, epsilon, matches);

        for (const uint32_t match_index : matches) {
            // Skip the position itself and any pair already covered in reverse
            if (match_index <= position_index) {
                continue;
            }

            neighbourhoods.make_union(position_index, match_index);
        }
    }

    return WeldingMap{
        .position_to_welded = neighbourhoods.get_set_labels(),
        .welded_position_count = neighbourhoods.set_count(),
    };
}

// Maps each position onto the welded position of all equal positions.
template <std::ranges::input_range Positions>
    requires std::ranges::sized_range<Positions>
WeldingMap build_exact_welding_map(Positions &&positions) {
    std::unordered_map<glm::dvec3, uint32_t, DVec3Hash> welded_by_position;
    std::vector<uint32_t> position_to_welded;
    position_to_welded.reserve(std::ranges::size(positions));

    for (const glm::dvec3 &position : positions) {
        const uint32_t next_welded_index = static_cast<uint32_t>(welded_by_position.size());
        const auto [it, inserted] = welded_by_position.try_emplace(position, next_welded_index);
        position_to_welded.push_back(it->second);
    }

    return WeldingMap{
        .position_to_welded = std::move(position_to_welded),
        .welded_position_count = static_cast<uint32_t>(welded_by_position.size()),
    };
}

inline void filter_degenerate_triangles(Clustering &clustering) {
    for (Cluster &cluster : clustering.clusters) {
        const size_t removed = std::erase_if(cluster.local_triangles, [&](const glm::uvec3 &local_triangle) {
            const glm::uvec3 global_triangle(
                cluster.vertex_indices[local_triangle.x],
                cluster.vertex_indices[local_triangle.y],
                cluster.vertex_indices[local_triangle.z]);

            return mesh::is_degenerate(global_triangle);
        });

        // Compact vertices if we found any.
        if (removed > 0) {
            compact_cluster_inplace(cluster);
        }
    }
}
} // namespace detail

// Maps each position onto the welded position of its transitive epsilon neighbourhood.
template <std::ranges::forward_range Positions>
    requires std::ranges::sized_range<Positions>
[[nodiscard]] WeldingMap build_welding_map(Positions &&positions, const double epsilon) {
    detail::validate_epsilon(epsilon);

    // The spatial hash of the epsilon weld cannot have a cell size of zero.
    if (epsilon == 0.0) {
        return detail::build_exact_welding_map(std::forward<Positions>(positions));
    }
    return detail::build_epsilon_welding_map(std::forward<Positions>(positions), epsilon);
}

// The welded positions, each averaged over its inputs or taken from the last one.
[[nodiscard]] inline std::vector<glm::dvec3> weld_positions(
    const std::span<const glm::dvec3> positions,
    const WeldingMap &map,
    const bool average_positions) {
    std::vector<glm::dvec3> welded_positions(map.welded_position_count, glm::dvec3(0.0));

    if (!average_positions) {
        for (const auto &[position_index, position] : enumerate(positions)) {
            welded_positions[map.position_to_welded[position_index]] = position;
        }

        return welded_positions;
    }

    std::vector<uint32_t> counts(map.welded_position_count, 0);
    for (const auto &[position_index, position] : enumerate(positions)) {
        const uint32_t welded_index = map.position_to_welded[position_index];
        welded_positions[welded_index] += position;
        counts[welded_index]++;
    }

    for (const auto &[welded_index, count] : enumerate(counts)) {
        DEBUG_ASSERT(count > 0);
        welded_positions[welded_index] /= static_cast<double>(count);
    }

    return welded_positions;
}

// Welds vertices of a single clustering that lie within epsilon of each other.
inline Clustering weld_clustering(const Clustering &clustering, const double epsilon, const bool average_positions = true) {
    detail::validate_epsilon(epsilon);

    if (clustering.is_empty()) {
        return clustering;
    }

    const WeldingMap welding_map = build_welding_map(clustering.positions, epsilon);

    Clustering welded = clustering;
    welded.positions = weld_positions(clustering.positions, welding_map, average_positions);

    for (Cluster &cluster : welded.clusters) {
        for (uint32_t &vertex_index : cluster.vertex_indices) {
            vertex_index = welding_map.position_to_welded[vertex_index];
        }
    }

    detail::filter_degenerate_triangles(welded);

    LOG_DEBUG(
        "Welding with {} welded and {} unique vertices",
        clustering.vertex_count() - welding_map.welded_position_count,
        welding_map.welded_position_count);

    validate(welded);
    return welded;
}