#pragma once

#include <bit>
#include <cstdint>

#include <glm/glm.hpp>

#include "geometry/bounds.h"
#include "numeric/wide_integer.h"

#include "bvh/layout.h"
#include "morton/code.h"

namespace morton {

// Counts the leading bits both codes share.
template <typename Code>
[[nodiscard]]
uint32_t count_common_prefix_bits(const Code &a, const Code &b) {
    const Code difference = a ^ b;
    if (difference == Code{0}) {
        return code_bits;
    }

    // Only the matching branch may be instantiated; the other does not compile for this type.
    if constexpr (is_wide_integer_v<Code>) {
        return code_bits - 1 - math::wide_integer::msb(difference);
    } else {
        return std::countl_zero(difference);
    }
}

// Morton code of `point` mapped into `scene`.
inline Code encode(const bvh::Bounds &scene, const glm::dvec3 &point) {
    constexpr CodeId::Level level = CodeId::max_level();
    const double max_coord = CodeId::max_coords_on_level(level).x;
    const glm::dvec3 relative_point = geometry::relativize_point(scene, point);
    const CodeId::Coords coords = relative_point * max_coord;
    return CodeId(level, coords).index_on_level();
}

// Morton code of a node's spatial position, taken at its bounds centre.
inline Code encode(const bvh::Bounds &scene, const bvh::NodeHeader &header) {
    return encode(scene, header.bounds.centre());
}

}
