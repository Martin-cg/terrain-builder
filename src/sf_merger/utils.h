#pragma once

#include <glm/common.hpp>
#include <radix/geometry.h>

#include "containers/Cow.h"
#include "geometry/geometry.h"
#include "mask.h"
#include "mesh/SimpleMesh.h"
#include "mesh/clip.h"
#include "mesh/texture_trim.h"

inline Cow<SimpleMesh> clip_on_mask(const SimpleMesh &mesh, const MeshMask &mask, const bool keep_inside = true) {
    Cow<SimpleMesh> clipped = mesh::clip_on_mesh(mesh, mask.mesh, keep_inside);
    clipped.visit(
        [](Cow<SimpleMesh>::Owned &owned) {
            trim_texture_inplace(owned.value);
        },
        [](const Cow<SimpleMesh>::Borrowed &) {});
    return clipped;
}
Cow<SimpleMesh> clip_on_mask(const SimpleMesh &&mesh, const MeshMask &mask, const bool keep_inside = true) = delete;
