#pragma once

#include <array>
#include <cstddef>
#include <stdexcept>
#include <type_traits>
#include <vector>

#include <glm/glm.hpp>
#include <ogr_spatialref.h>

#include "srs/transformation.h"

namespace srs {

template <typename T>
inline glm::tvec2<T> transform_point(OGRCoordinateTransformation *transform, glm::tvec2<T> p) {
    if (!transform->Transform(1, &p.x, &p.y))
        throw std::runtime_error("srs::transform_point(glm::tvec2<T>) failed");
    return p;
}
template <typename T>
inline glm::tvec3<T> transform_point(OGRCoordinateTransformation *transform, glm::tvec3<T> p) {
    if (!transform->Transform(1, &p.x, &p.y, &p.z))
        throw std::runtime_error("srs::transform_point(glm::tvec3<T>) failed");
    return p;
}

template <typename T>
inline glm::tvec2<T> transform_point(const OGRSpatialReference &source_srs, const OGRSpatialReference &target_srs, glm::tvec2<T> p) {
    const auto transform = transformation(source_srs, target_srs);
    return transform_point(transform.get(), p);
}
template <typename T>
inline glm::tvec3<T> transform_point(const OGRSpatialReference &source_srs, const OGRSpatialReference& target_srs, glm::tvec3<T> p){
    const auto transform = transformation(source_srs, target_srs);
    return transform_point(transform.get(), p);
}

template <typename Container>
inline void transform_points_inplace(OGRCoordinateTransformation *transform, Container &points) {
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
        throw std::runtime_error("srs::transform_points_inplace failed");
    }

    for (size_t i = 0; i < size; i++) {
        if constexpr (is_3d) {
            points[i] = PointType(xs[i], ys[i], zs[i]);
        } else {
            points[i] = PointType(xs[i], ys[i]);
        }
    }
}

template <typename Container>
inline Container transform_points(const OGRSpatialReference &source_srs, const OGRSpatialReference &target_srs, Container points) {
    const auto transform = transformation(source_srs, target_srs);
    transform_points_inplace(transform.get(), points);
    return points;
}


// TODO: somehow integrate into a single transform_points
template <typename T>
inline std::vector<glm::tvec2<T>> transform_points_to_2d(OGRCoordinateTransformation *transform, const std::vector<glm::tvec3<T>> &points) {
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
        throw std::runtime_error("srs::transform_points_to_2d(OGRCoordinateTransformation *, std::vector<glm::tvec3<T>>) failed");
    }

    std::vector<glm::tvec2<T>> transformed;
    transformed.resize(points.size());
    for (size_t i = 0; i < points.size(); ++i) {
        transformed[i] = {xs[i], ys[i]};
    }

    return transformed;
}

}
