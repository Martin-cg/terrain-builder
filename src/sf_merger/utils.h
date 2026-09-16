#pragma once

#include <optional>

#include <glm/common.hpp>
#include <radix/geometry.h>

#include "containers/Cow.h"
#include "geometry/geometry.h"
#include "mask.h"
#include "mesh/SimpleMesh.h"
#include "mesh/clip.h"
#include "mesh/combine.h"
#include "mesh/texture_trim.h"

inline MeshMask clip_mask_on_bounds(const MeshMask &mask, const radix::geometry::Aabb3d &bounds) {
    MeshMask result;
    result.components.reserve(mask.components.size());

    for (const SimpleMesh &component : mask.components) {
        Cow<SimpleMesh> clipped = mesh::clip_on_bounds_and_cap(component, bounds);
        if (!clipped->is_empty()) {
            result.components.push_back(std::move(clipped).into_owned());
        }
    }

    return result;
}

inline Cow<SimpleMesh> clip_on_mask(const SimpleMesh &mesh, const MeshMask &mask, const bool keep_inside = true) {
    if (keep_inside) {
        std::vector<SimpleMesh> clipped_components;
        clipped_components.reserve(mask.components.size());

        for (const SimpleMesh &component : mask.components) {
            Cow<SimpleMesh> clipped = mesh::clip_on_mesh(mesh, component, true);
            if (clipped.is_borrowed()) {
                return Cow<SimpleMesh>::borrowed(mesh);
            }
            if (!clipped->is_empty()) {
                clipped_components.push_back(std::move(clipped).into_owned());
            }
        }

        if (clipped_components.empty()) {
            return Cow<SimpleMesh>::owned(SimpleMesh());
        }

        SimpleMesh result = clipped_components.size() == 1
            ? std::move(clipped_components.front())
            : mesh::combine(clipped_components);
        result.texture = mesh.texture;
        trim_texture_inplace(result);
        return Cow<SimpleMesh>::owned(std::move(result));
    }

    std::optional<SimpleMesh> result;
    const SimpleMesh *current = &mesh;
    for (const SimpleMesh &component : mask.components) {
        Cow<SimpleMesh> clipped = mesh::clip_on_mesh(*current, component, false);
        if (!clipped.is_borrowed()) {
            result = std::move(clipped).into_owned();
            current = &result.value();
            if (current->is_empty()) {
                break;
            }
        }
    }

    if (!result.has_value()) {
        return Cow<SimpleMesh>::borrowed(mesh);
    }

    trim_texture_inplace(result.value());
    return Cow<SimpleMesh>::owned(std::move(result.value()));
}
Cow<SimpleMesh> clip_on_mask(const SimpleMesh &&mesh, const MeshMask &mask, const bool keep_inside = true) = delete;
