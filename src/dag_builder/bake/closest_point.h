#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include <glm/glm.hpp>
#include <opencv2/core.hpp>
#include <radix/geometry.h>

#include "bake/UvGeometry.h"
#include "bake/query.h"
#include "bake/render_points.h"
#include "correspondence.h"
#include "geometry/geometry.h"
#include "texture/RenderTarget.h"

namespace bake {

// Take each texel from the nearest point on the source surface.
struct ClosestPoint {
    std::optional<double> max_distance = {};
    // Leaves a texel uncovered rather than taking a source that faces away from it.
    bool require_facing = false;
};

namespace detail {

inline void render(
    const BakeGeometry &geometry,
    const Correspondence &correspondence,
    const texture::RenderTarget target,
    const int interpolation,
    const ClosestPoint &mode) {
    render_points(geometry, correspondence, target, interpolation, [&](const PointQuery &query) {
        return find_closest_point(query.candidates, query.point, mode.max_distance, mode.require_facing);
    });
}

} // namespace detail

} // namespace bake
