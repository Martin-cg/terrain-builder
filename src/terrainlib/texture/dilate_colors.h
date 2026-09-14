#pragma once

#include <cstdint>
#include <optional>
#include <span>

#include <glm/glm.hpp>
#include <opencv2/core.hpp>

#include "texture/Mask.h"

namespace texture {

// Grows the covered region into the target, each new texel taking the mean of its covered
// neighbours. Runs until the target is full, or for max_radius rounds. Returns the texels filled.
uint32_t dilate_colors_inplace(cv::Mat &image, Mask &coverage, const Mask &target, std::optional<uint32_t> max_radius = {});

// Grows the covered region by radius texels, each new texel taking the mean of its covered neighbours.
void dilate_colors_inplace(cv::Mat &image, Mask &coverage, const uint32_t radius);

// Grows the covered region by radius texels, each new texel taking the mean of its covered neighbours.
cv::Mat dilate_colors(const cv::Mat &image, const Mask &coverage, const uint32_t radius);

// Fills the uncovered texels inside the given uv triangles, each from the covered texels around
// its hole, weighted by inverse square distance. Returns the texels filled.
uint32_t interpolate_uncovered(
    cv::Mat &image,
    Mask &coverage,
    const std::span<const glm::uvec3> triangles,
    const std::span<const glm::dvec2> uvs);

} // namespace texture
