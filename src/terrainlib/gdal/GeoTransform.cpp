#include <gdal.h>
#include <gdal_priv.h>

#include "error/utils.h"
#include "gdal/GeoTransform.h"

namespace gdal {

alp::Result<GeoTransform, GeoTransformError> GeoTransform::from_forward(const std::array<double, 6> &forward) {
    std::array<double, 6> inverse;
    if (GDALInvGeoTransform(forward.data(), inverse.data()) != TRUE) {
        return alp::make_unexpected(GeoTransformError::NotInvertible);
    }
    return GeoTransform(forward, inverse);
}

alp::Result<GeoTransform, GeoTransformError> GeoTransform::from_dataset(GDALDataset &dataset) {
    std::array<double, 6> forward;
    if (dataset.GetGeoTransform(forward.data()) != CE_None) {
        return alp::make_unexpected(GeoTransformError::Missing);
    }
    return GeoTransform::from_forward(forward);
}

glm::dvec2 GeoTransform::apply(const glm::dvec2 pixel) const {
    glm::dvec2 point;
    GDALApplyGeoTransform(this->_forward.data(), pixel.x, pixel.y, &point.x, &point.y);
    return point;
}

glm::dvec2 GeoTransform::apply_inverse(const glm::dvec2 point) const {
    glm::dvec2 pixel;
    GDALApplyGeoTransform(this->_inverse.data(), point.x, point.y, &pixel.x, &pixel.y);
    return pixel;
}

bool GeoTransform::is_axis_aligned() const {
    return this->_forward[2] == 0.0 && this->_forward[4] == 0.0;
}

} // namespace gdal
