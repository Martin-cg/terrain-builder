#pragma once

#include <radix/geometry.h>

#include "mesh/SimpleMesh.h"
#include "containers/Cow.h"

namespace mesh {

Cow<SimpleMesh> clip_on_bounds(const SimpleMesh &mesh, const radix::geometry::Aabb3d &bounds);
Cow<SimpleMesh> clip_on_bounds(const SimpleMesh &&mesh, const radix::geometry::Aabb3d &bounds) = delete;
Cow<SimpleMesh> clip_on_bounds_and_cap(
    const SimpleMesh &mesh,
    const radix::geometry::Aabb3d &bounds,
    const bool remesh_planar_patches = true);
Cow<SimpleMesh> clip_on_bounds_and_cap(
    const SimpleMesh &&mesh,
    const radix::geometry::Aabb3d &bounds,
    const bool remesh_planar_patches = true) = delete;
Cow<SimpleMesh> clip_on_mesh(const SimpleMesh &mesh, const SimpleMesh& clip_mesh, const bool keep_inside = true);
Cow<SimpleMesh> clip_on_mesh(const SimpleMesh &&mesh, const SimpleMesh& clip_mesh, const bool keep_inside = true) = delete;

}
