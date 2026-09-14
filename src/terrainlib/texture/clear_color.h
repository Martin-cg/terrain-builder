#pragma once

#include <opencv2/core.hpp>

#include "build_config.h"

// Magenta where asserts are on, so texels a bake never wrote stand out.
inline const cv::Scalar DEFAULT_CLEAR_COLOR = IS_DEBUG_BUILD ? cv::Scalar(255, 0, 255) : cv::Scalar::all(0);