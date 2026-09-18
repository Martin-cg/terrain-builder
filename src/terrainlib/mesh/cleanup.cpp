#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <ranges>
#include <span>
#include <unordered_map>
#include <utility>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtx/hash.hpp>

#include "containers/SegmentedBuffer.h"
#include "enumerate.h"
#include "mesh/cleanup.h"
#include "mesh/normalize.h"
#include "mesh/connectivity/adjacency.h"

namespace mesh {

namespace {
struct KeyEntry {
    uint32_t first;
    uint32_t group;
};
} // namespace

SegmentedBuffer<uint32_t, uint32_t> find_duplicate_triangle_groups(const std::span<const glm::uvec3> triangles, const bool ignore_orientation) {
    constexpr uint32_t no_group = std::numeric_limits<uint32_t>::max();

    std::unordered_map<glm::uvec3, KeyEntry> entries;
    entries.reserve(triangles.size());
    std::vector<std::pair<uint32_t, uint32_t>> pairs;
    uint32_t group_count = 0;
    for (const auto [triangle_index, triangle] : enumerate<uint32_t>(triangles)) {
        const glm::uvec3 key = normalize_triangle(triangle, !ignore_orientation);
        const auto [it, inserted] = entries.try_emplace(key, KeyEntry{triangle_index, no_group});
        if (inserted) {
            continue;
        }

        KeyEntry &entry = it->second;
        if (entry.group == no_group) {
            entry.group = group_count;
            group_count++;
            pairs.emplace_back(entry.group, entry.first);
        }
        pairs.emplace_back(entry.group, triangle_index);
    }

    return group_by_key<uint32_t, uint32_t>(group_count, pairs);
}

std::vector<uint32_t> find_duplicate_triangles_consider_orientation(const std::span<const glm::uvec3> triangles) {
    const SegmentedBuffer<uint32_t, uint32_t> groups = find_duplicate_triangle_groups(triangles, false);

    std::vector<uint32_t> duplicates;
    for (uint32_t group_index = 0; group_index < groups.segment_count(); group_index++) {
        for (const uint32_t triangle_index : groups.segment(group_index) | std::views::drop(1)) {
            duplicates.push_back(triangle_index);
        }
    }
    std::ranges::sort(duplicates);

    return duplicates;
}

void remove_duplicate_triangles_consider_orientation(std::vector<glm::uvec3> &triangles) {
    remove_duplicate_triangles<double>(triangles, std::span<const glm::dvec3>(), false);
}

void remove_degenerate_triangles(std::vector<glm::uvec3> &triangles) {
    triangles.erase(
        std::remove_if(triangles.begin(), triangles.end(), is_degenerate),
        triangles.end());
}

}
