#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <span>
#include <vector>

#include <glm/glm.hpp>
#include <opencv2/core.hpp>
#include <radix/geometry.h>

#include "bake/UvTriangle.h"
#include "cluster.h"
#include "cluster_views.h"
#include "geometry/geometry.h"
#include "range_utils.h"

namespace bake {

// A triangle set with the uvs its corners carry.
struct UvGeometry {
    std::vector<glm::uvec3> triangles; // global vertex indices
    std::vector<UvTriangle> uvs; // per triangle corner
};

// Geometry whose uvs address images, which is what a bake reads back through.
struct TexturedGeometry {
    UvGeometry geometry;
    std::vector<uint32_t> image_indices; // per triangle
    std::vector<cv::Mat> images;
};

// The two surfaces a bake relates, over the vertex space they share.
struct BakeGeometry {
    UvGeometry target;
    TexturedGeometry source;
    std::span<const glm::dvec3> positions; // indexed by both triangle sets
};

// Clamps every corner into the unit square.
inline void clamp_uvs(const std::span<UvTriangle> uvs) {
    for (UvTriangle &triangle : uvs) {
        for (glm::dvec2 &uv : triangle) {
            uv = glm::clamp(uv, 0.0, 1.0);
        }
    }
}

// Gather the given clusters into one triangle set.
[[nodiscard]]
inline UvGeometry make_uv_geometry(const ClusteringView view) {
    const uint32_t triangle_count = sum(view.clusters, [&](const uint32_t index) {
        return view.clustering.clusters[index].triangle_count();
    });

    UvGeometry uv_geometry;
    uv_geometry.triangles.reserve(triangle_count);
    uv_geometry.uvs.reserve(triangle_count);

    for (const uint32_t cluster_index : view.clusters) {
        const Cluster &cluster = view.clustering.clusters[cluster_index];
        ASSERT(cluster.has_uvs());

        for (const glm::uvec3 &local : cluster.local_triangles) {
            uv_geometry.triangles.push_back(cluster.global_triangle(local));
        }
        for (const glm::uvec3 &local : cluster.local_triangles) {
            uv_geometry.uvs.push_back(geometry::corners(local, cluster.uvs));
        }
    }

    clamp_uvs(uv_geometry.uvs);

    return uv_geometry;
}

// Gather the given clusters into one triangle set, with the textures their uvs address.
[[nodiscard]]
inline TexturedGeometry make_textured_geometry(const ClusteringView view) {
    TexturedGeometry textured;
    textured.geometry = make_uv_geometry(view);
    textured.image_indices.reserve(textured.geometry.triangles.size());
    textured.images.reserve(view.clusters.size());

    for (const uint32_t cluster_index : view.clusters) {
        const Cluster &cluster = view.clustering.clusters[cluster_index];
        ASSERT(cluster.is_textured());

        const uint32_t image_index = textured.images.size();
        textured.images.push_back(view.clustering.textures[cluster.texture_id.value()]);
        textured.image_indices.insert(textured.image_indices.end(), cluster.triangle_count(), image_index);
    }

    return textured;
}

// Gather the surfaces a bake reads and writes, which must index one shared position array.
[[nodiscard]]
inline BakeGeometry make_bake_geometry(const ClusteringView target, const ClusteringView source) {
    // Merging shares the position array and simplification copies it whole, so one span serves both.
    DEBUG_ASSERT(std::ranges::equal(target.clustering.positions, source.clustering.positions));

    return BakeGeometry{
        .target = make_uv_geometry(target),
        .source = make_textured_geometry(source),
        .positions = target.clustering.positions};
}

} // namespace bake