#pragma once

#include <algorithm>

#include <glm/glm.hpp>
#include <opencv2/core.hpp>

#include "cluster.h"

// Gives every cluster without UVs dummy UVs, and every cluster without a texture a dummy texture.
inline void fill_missing_textures_inplace(Clustering &clustering) {
    for (Cluster &cluster : clustering.clusters) {
        if (!cluster.has_uvs()) {
            cluster.uvs.assign(cluster.vertex_count(), glm::dvec2{0.0});
        }
    }

    const bool any_untextured = std::ranges::any_of(clustering.clusters, [](const Cluster &c) { return !c.is_textured(); });
    if (!any_untextured) {
        return;
    }

    // An existing texture serves as the dummy when there is one.
    if (clustering.textures.empty()) {
        const cv::Mat dummy_texture = cv::Mat::zeros(1, 1, CV_8UC3);
        clustering.textures.add(dummy_texture);
    }
    for (Cluster &cluster : clustering.clusters) {
        if (!cluster.is_textured()) {
            cluster.texture_id = 0;
        }
    }
}