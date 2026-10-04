#pragma once

#include <array>
#include <cstddef>
#include <type_traits>
#include <vector>

#include <glm/glm.hpp>
#include <libassert/assert.hpp>
#include <ogr_spatialref.h>

#include "error/Result.h"
#include "error/try.h"
#include "error/utils.h"
#include "srs.h"
#include "srs/error.h"
#include "srs/transformation.h"

namespace srs {

namespace detail {

// Creates the code for a point that `transform` cannot map.
inline UntransformablePoint make_untransformable_point(const OGRCoordinateTransformation *transform) {
    const OGRSpatialReference *source_srs = ASSERT_VAL(transform->GetSourceCS());
    const OGRSpatialReference *target_srs = ASSERT_VAL(transform->GetTargetCS());
    return UntransformablePoint{.source = friendly_name(*source_srs), .target = friendly_name(*target_srs)};
}

} // namespace detail

template <typename T>
[[nodiscard]]
inline alp::Result<glm::tvec2<T>, UntransformablePoint> transform_point(OGRCoordinateTransformation *transform, glm::tvec2<T> p) {
    if (!transform->Transform(1, &p.x, &p.y)) {
        return alp::make_unexpected(detail::make_untransformable_point(transform));
    }
    return p;
}
template <typename T>
[[nodiscard]]
inline alp::Result<glm::tvec3<T>, UntransformablePoint> transform_point(OGRCoordinateTransformation *transform, glm::tvec3<T> p) {
    if (!transform->Transform(1, &p.x, &p.y, &p.z)) {
        return alp::make_unexpected(detail::make_untransformable_point(transform));
    }
    return p;
}

template <typename T>
[[nodiscard]]
inline alp::Result<glm::tvec2<T>, TransformError> transform_point(const OGRSpatialReference &source_srs, const OGRSpatialReference &target_srs, glm::tvec2<T> p) {
    ALP_TRY(const auto transform, transformation(source_srs, target_srs));
    return transform_point(transform.get(), p);
}
template <typename T>
[[nodiscard]]
inline alp::Result<glm::tvec3<T>, TransformError> transform_point(const OGRSpatialReference &source_srs, const OGRSpatialReference& target_srs, glm::tvec3<T> p){
    ALP_TRY(const auto transform, transformation(source_srs, target_srs));
    return transform_point(transform.get(), p);
}

template <typename Container>
[[nodiscard]]
inline alp::Result<void, UntransformablePoint> transform_points_inplace(OGRCoordinateTransformation *transform, Container &points) {
    using PointType = typename Container::value_type;
    using T = typename PointType::value_type;
    constexpr bool is_3d = (PointType::length() == 3);
    constexpr bool is_array = std::is_array_v<Container>;

    const size_t size = points.size();

    constexpr std::size_t array_size = is_array ? points.size() : 0;
    using OutputContainer = std::conditional_t<is_array, std::array<T, array_size>, std::vector<T>>;

    OutputContainer xs, ys;
    std::conditional_t<is_3d, OutputContainer, int> zs;
    
    if constexpr (!is_array) {
        xs.resize(size);
        ys.resize(size);
        if constexpr (PointType::length() == 3) {
            zs.resize(size);
        }
    }

    for (size_t i = 0; i < size; i++) {
        xs[i] = points[i].x;
        ys[i] = points[i].y;
        if constexpr (is_3d) {
            zs[i] = points[i].z;
        }
    }

    bool success;
    if constexpr (is_3d) {
        success = transform->Transform(size, xs.data(), ys.data(), zs.data());
    } else {
        success = transform->Transform(size, xs.data(), ys.data());
    }

    if (!success) {
        return alp::make_unexpected(detail::make_untransformable_point(transform));
    }

    for (size_t i = 0; i < size; i++) {
        if constexpr (is_3d) {
            points[i] = PointType(xs[i], ys[i], zs[i]);
        } else {
            points[i] = PointType(xs[i], ys[i]);
        }
    }
    return {};
}

template <typename Container>
[[nodiscard]]
inline alp::Result<Container, TransformError> transform_points(const OGRSpatialReference &source_srs, const OGRSpatialReference &target_srs, Container points) {
    ALP_TRY(const auto transform, transformation(source_srs, target_srs));
    ALP_TRY(transform_points_inplace(transform.get(), points));
    return points;
}


// TODO: somehow integrate into a single transform_points
template <typename T>
[[nodiscard]]
inline alp::Result<std::vector<glm::tvec2<T>>, UntransformablePoint> transform_points_to_2d(OGRCoordinateTransformation *transform, const std::vector<glm::tvec3<T>> &points) {
    std::vector<T> xs;
    std::vector<T> ys;
    std::vector<T> zs;

    xs.resize(points.size());
    ys.resize(points.size());
    zs.resize(points.size());

    for (size_t i = 0; i < points.size(); i++) {
        xs[i] = points[i].x;
        ys[i] = points[i].y;
        zs[i] = points[i].z;
    }

    if (!transform->Transform(points.size(), xs.data(), ys.data(), zs.data())) {
        return alp::make_unexpected(detail::make_untransformable_point(transform));
    }

    std::vector<glm::tvec2<T>> transformed;
    transformed.resize(points.size());
    for (size_t i = 0; i < points.size(); ++i) {
        transformed[i] = {xs[i], ys[i]};
    }

    return transformed;
}

}
