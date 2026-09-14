#pragma once

#include "geometry/geometry.h"

namespace bake {

// A triangle in texture space, its corners in [0, 1] uv coordinates.
using UvTriangle = geometry::Triangle2d;

} // namespace bake
