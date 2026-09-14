#pragma once

#include <stdexcept>

#include <glm/glm.hpp>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

#include "opencv_utils.h"

namespace texture {

// Clamp-to-edge: out-of-bounds coordinates reuse the nearest border pixel.
template <int channels, typename Value>
cv::Vec<Value, channels> read_texel(const cv::Mat &image, const glm::ivec2 pos) {
    const glm::ivec2 clamped = glm::clamp(pos, glm::ivec2(0), glm::ivec2(image.cols - 1, image.rows - 1));
    return image.at<cv::Vec<Value, channels>>(clamped.y, clamped.x);
}

template <typename AsValue, int channels, typename Value>
cv::Vec<AsValue, channels> read_texel_as(const cv::Mat &image, const glm::ivec2 pos) {
    return convert_texel<AsValue>(read_texel<channels, Value>(image, pos));
}

template <int channels, typename Value>
cv::Vec<float, channels> sample_linear(const cv::Mat &image, const glm::dvec2 &point) {
    const glm::ivec2 p0 = glm::ivec2(glm::floor(point));
    const glm::dvec2 t = point - glm::dvec2(p0);

    const auto c00 = read_texel_as<float, channels, Value>(image, p0);
    const auto c10 = read_texel_as<float, channels, Value>(image, p0 + glm::ivec2(1, 0));
    const auto c01 = read_texel_as<float, channels, Value>(image, p0 + glm::ivec2(0, 1));
    const auto c11 = read_texel_as<float, channels, Value>(image, p0 + glm::ivec2(1, 1));

    return c00 * static_cast<float>((1.0 - t.x) * (1.0 - t.y)) +
           c10 * static_cast<float>(t.x * (1.0 - t.y)) +
           c01 * static_cast<float>((1.0 - t.x) * t.y) +
           c11 * static_cast<float>(t.x * t.y);
}

template <int channels, typename Value>
cv::Vec<float, channels> sample_nearest(const cv::Mat &image, const glm::dvec2 &point) {
    return read_texel_as<float, channels, Value>(image, glm::ivec2(glm::round(point)));
}

// Reads the image at a point in texel-index space with the given interpolation.
template <int channels, typename Value>
cv::Vec<float, channels> sample_image(
    const cv::Mat &image,
    const glm::dvec2 &point,
    const int interpolation) {
    switch (interpolation) {
    case cv::INTER_NEAREST:
        return sample_nearest<channels, Value>(image, point);

    case cv::INTER_LINEAR:
        return sample_linear<channels, Value>(image, point);

    default:
        throw std::invalid_argument("sample_image supports INTER_NEAREST and INTER_LINEAR");
    }
}

} // namespace texture