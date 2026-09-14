#pragma once

#include <cstdint>
#include <optional>

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

} // namespace texture
