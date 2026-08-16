#pragma once

#include <glm/glm.hpp>

#include "geometry/geometry.h"
#include "numeric/number_types.h"
#include "octree/Id.h"

#include "bvh.h"

namespace frontier {

using MortonCode = uint_for_bits_t<128>;

// Morton code of `point` mapped into `scene`.
inline MortonCode morton_of(const bvh::Bounds &scene, const glm::dvec3 &point) {
    constexpr octree::Id::Level level = octree::Id::max_level();
    const double max_coord = octree::Id::max_coords_on_level(level).x;
    const glm::dvec3 relative_point = geometry::relativize_point(scene, point);
    const octree::Id::Coords coords = relative_point * max_coord;
    return octree::Id(level, coords).index_on_level();
}

// Morton code of a node's spatial position, taken at its bounds centre.
inline MortonCode morton_of(const bvh::Bounds &scene, const bvh::NodeHeader &header) {
    return morton_of(scene, header.bounds.centre());
}

}