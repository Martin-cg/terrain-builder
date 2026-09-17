#pragma once

#include <optional>
#include <span>

#include <glm/glm.hpp>
#include <glm/gtx/norm.hpp>
#include <radix/geometry.h>

#include "geometry/CoordFrame.h"
#include "geometry/geometry.h"

// A plane carrying an in-plane coordinate system, so geometry can be laid out on it.
template <typename T = double>
struct PlaneFrame_ : CoordFrame_<T> {
    // Empty for a triangle thinner than epsilon, whose normal is unreliable.
    static std::optional<PlaneFrame_> from_triangle(const radix::geometry::Triangle<3, T> &triangle) {
        // The smallest altitude never exceeds the shortest edge, so this also rejects every near-zero edge.
        if (geometry::compute_triangle_thickness(triangle) <= geometry::epsilon<T>()) {
            return std::nullopt;
        }

        PlaneFrame_ frame;
        frame.origin = triangle[0];
        frame.normal = geometry::compute_normal(triangle);
        frame.tangent = glm::normalize(triangle[1] - triangle[0]);
        frame.bitangent = glm::cross(frame.normal, frame.tangent);

        return frame;
    }

    static std::optional<PlaneFrame_> from_triangle(const glm::uvec3 &triangle, const std::span<const glm::vec<3, T>> positions) {
        return from_triangle(geometry::corners(triangle, positions));
    }

    // Signed distance from the plane, positive on the side the normal points to.
    T distance_to(const glm::vec<3, T> &position) const {
        return this->to_local(position).z;
    }

    // The position within the plane, relative to the origin.
    glm::vec<2, T> project(const glm::vec<3, T> &position) const {
        return glm::vec<2, T>(this->to_local(position));
    }

    // The triangle laid onto the plane. Counter-clockwise when the frame was built from it.
    radix::geometry::Triangle<2, T> flatten(const radix::geometry::Triangle<3, T> &triangle) const {
        return {
            this->project(triangle[0]),
            this->project(triangle[1]),
            this->project(triangle[2]),
        };
    }

    radix::geometry::Triangle<2, T> flatten(const glm::uvec3 &triangle, const std::span<const glm::vec<3, T>> positions) const {
        return this->flatten(geometry::corners(triangle, positions));
    }
};

using PlaneFrame = PlaneFrame_<double>;