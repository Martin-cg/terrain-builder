#pragma once

#include <optional>

#include <libassert/assert.hpp>
#include <opencv2/imgproc.hpp>

namespace texture {

// How an image is read between its texels.
enum class Interpolation {
    Nearest,
    Linear,
};

// The OpenCV interpolation flag that reads the same way.
[[nodiscard]]
inline int to_opencv_flag(const Interpolation interpolation) {
    switch (interpolation) {
    case Interpolation::Nearest:
        return cv::INTER_NEAREST;
    case Interpolation::Linear:
        return cv::INTER_LINEAR;
    }
    UNREACHABLE();
}

// Finds the interpolation that reads the way the OpenCV flag does. Empty for a flag with no counterpart.
[[nodiscard]]
inline std::optional<Interpolation> try_from_opencv_flag(const int flag) {
    switch (flag) {
    case cv::INTER_NEAREST:
        return Interpolation::Nearest;
    case cv::INTER_LINEAR:
        return Interpolation::Linear;
    default:
        return std::nullopt;
    }
}

} // namespace texture
