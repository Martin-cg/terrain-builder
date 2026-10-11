#pragma once

#include <glm/glm.hpp>

#include "geometry/bounds.h"
#include "numeric/number_types.h"
#include "octree/Id.h"

#include "bvh/layout.h"

namespace morton {

using CodeId = octree::Id_<3, 128 / 3>;
using Code = CodeId::Index;

constexpr uint32_t code_bits = std::numeric_limits<Code>::digits;

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