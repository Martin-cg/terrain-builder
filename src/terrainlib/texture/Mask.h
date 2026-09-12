#pragma once

#include <cstdint>
#include <limits>
#include <type_traits>

#include <glm/glm.hpp>
#include <opencv2/core.hpp>

namespace texture {

namespace detail {

// What a mask stored as T holds where set: 1 for floating point, the largest value otherwise.
template <typename T>
constexpr T get_set_value() {
    if constexpr (std::is_floating_point_v<T>) {
        return T{1};
    } else {
        return std::numeric_limits<T>::max();
    }
}

// Whether a value stored in a mask marks its texel as set.
template <typename T>
constexpr bool is_set(const T value) {
    return value != T{0};
}

} // namespace detail

// A boolean mask over an image, stored as T: zero where clear and non-zero where set.
template <typename T>
class Mask_ {
public:
    explicit Mask_(const glm::uvec2 size)
        : _mask(size.y, size.x, cv::DataType<T>::type, cv::Scalar::all(0)) {
    }

    // The same mask stored as T.
    template <typename U>
    explicit Mask_(const Mask_<U> &other) : Mask_(other.size()) {
        const cv::Mat is_set = static_cast<const cv::Mat &>(other) != 0;
        this->_mask.setTo(cv::Scalar::all(detail::get_set_value<T>()), is_set);
    }

    bool get(const int x, const int y) const {
        return detail::is_set(this->_mask.at<T>(y, x));
    }

    void set(const int x, const int y, const bool value = true) {
        this->_mask.at<T>(y, x) = value ? detail::get_set_value<T>() : T{0};
    }

    glm::uvec2 size() const {
        return {this->_mask.cols, this->_mask.rows};
    }

    uint32_t count() const {
        return cv::countNonZero(this->_mask);
    }

    Mask_ clone() const {
        Mask_ copy(this->size());
        this->_mask.copyTo(copy._mask);
        return copy;
    }

    // The mask as an image of T, holding the set value where set and 0 elsewhere.
    operator const cv::Mat &() const {
        return this->_mask;
    }

private:
    cv::Mat _mask;
};

using Mask = Mask_<uint8_t>;

} // namespace texture