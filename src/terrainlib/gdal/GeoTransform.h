#pragma once

#include <array>
#include <string_view>

#include <glm/vec2.hpp>

#include "error/Result.h"

class GDALDataset;

namespace gdal {

// Why a dataset has no usable geotransform.
enum class GeoTransformError {
    Missing,
    NotInvertible
};

inline std::string_view format_as(const GeoTransformError error) {
    switch (error) {
    case GeoTransformError::Missing:
        return "the dataset has no geotransform";
    case GeoTransformError::NotInvertible:
        return "the geotransform cannot be inverted";
    default:
        return "an unknown geotransform failure";
    }
}

// The affine transform from pixel coordinates into a dataset's srs, and its inverse.
class GeoTransform {
public:
    // Creates the transform from GDAL's six coefficients, failing if they cannot be inverted.
    [[nodiscard]]
    static alp::Result<GeoTransform, GeoTransformError> from_forward(const std::array<double, 6> &forward);

    // Reads the transform of `dataset`, failing if it has none or it cannot be inverted.
    [[nodiscard]]
    static alp::Result<GeoTransform, GeoTransformError> from_dataset(GDALDataset &dataset);

    bool operator==(const GeoTransform &) const = default;

    // The point in the dataset's srs that `pixel` maps to.
    [[nodiscard]]
    glm::dvec2 apply(glm::dvec2 pixel) const;

    // The pixel coordinates that `point` in the dataset's srs maps to.
    [[nodiscard]]
    glm::dvec2 apply_inverse(glm::dvec2 point) const;

    // Whether the transform has neither shear nor rotation.
    [[nodiscard]]
    bool is_axis_aligned() const;

private:
    explicit GeoTransform(const std::array<double, 6> &forward, const std::array<double, 6> &inverse) : _forward(forward), _inverse(inverse) {}

    std::array<double, 6> _forward;
    std::array<double, 6> _inverse;
};

} // namespace gdal
