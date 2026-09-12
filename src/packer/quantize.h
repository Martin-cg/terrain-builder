#pragma once

#include <cstdint>

#include <glm/glm.hpp>

#include "fixed.h"
#include "glm_utils.h"

namespace packer {

using FixedPoint = fixed::Q<10, int64_t>;

// The distance between neighbouring grid points, in the units of a point.
inline constexpr double GRID_STEP = 1.0 / FixedPoint::scale;

using QuantizedPoint = glm::vec<3, FixedPoint::repr_t>;

[[nodiscard]] inline QuantizedPoint quantize_point(const glm::dvec3 &point) {
    return transform(point, [](const double v) {
        return FixedPoint::from_real(v).value;
    });
}

[[nodiscard]] inline glm::dvec3 dequantize_point(const QuantizedPoint quantized) {
    return transform(quantized, [](const FixedPoint::repr_t v) {
        return FixedPoint{v}.real();
    });
}

}