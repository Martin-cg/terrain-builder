#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>

#include <glm/glm.hpp>
#include <libassert/assert.hpp>
#include <radix/geometry.h>

// One of the six directions along the coordinate axes.
class SignedAxis {
public:
    // Makes the direction along the axis with `index`, towards larger coordinates if `sign` is 1 and smaller ones if it is -1.
    SignedAxis(const glm::length_t index, const double sign) {
        DEBUG_ASSERT(index >= 0 && index < 3);
        DEBUG_ASSERT(sign == 1.0 || sign == -1.0);
        this->_value = static_cast<uint8_t>(index * 2 + (sign > 0 ? 1 : 0));
    }

    // Gets the index of the coordinate along this axis.
    glm::length_t get_index() const {
        return this->_value / 2;
    }
    // Gets the sign of this direction, -1 or 1.
    double get_sign() const {
        return this->_value % 2 == 1 ? 1.0 : -1.0;
    }

    bool operator==(const SignedAxis &) const = default;

private:
    friend struct std::hash<SignedAxis>;

    uint8_t _value;
};

namespace std {
template <>
struct hash<SignedAxis> {
    size_t operator()(const SignedAxis &axis) const noexcept {
        return std::hash<uint8_t>()(axis._value);
    }
};
} // namespace std

// An axis-aligned plane, measured like a radix plane whose normal is `normal`.
struct AxisAlignedPlane {
    SignedAxis normal;
    double distance;

    bool operator==(const AxisAlignedPlane &) const = default;
};

namespace geometry {

// Measures the signed distance of `position` from `plane`, positive on the side its normal points to.
inline double compute_distance(const AxisAlignedPlane &plane, const glm::dvec3 &position) {
    return plane.normal.get_sign() * position[plane.normal.get_index()] + plane.distance;
}

// Projects `position` onto `plane`, with the coordinate along its normal set exactly to the plane's.
inline glm::dvec3 project_onto_plane(const glm::dvec3 &position, const AxisAlignedPlane &plane) {
    // Solving sign * x + distance = 0 for x only multiplies by the sign, which is exact.
    glm::dvec3 projected = position;
    projected[plane.normal.get_index()] = -plane.normal.get_sign() * plane.distance;
    return projected;
}

// Constructs the six planes of the faces of `bounds`, with normals facing inwards.
inline std::array<AxisAlignedPlane, 6> construct_face_planes(const radix::geometry::Aabb3d &bounds) {
    return {
        AxisAlignedPlane{SignedAxis(0, 1.0), -bounds.min.x},
        AxisAlignedPlane{SignedAxis(0, -1.0), bounds.max.x},
        AxisAlignedPlane{SignedAxis(1, 1.0), -bounds.min.y},
        AxisAlignedPlane{SignedAxis(1, -1.0), bounds.max.y},
        AxisAlignedPlane{SignedAxis(2, 1.0), -bounds.min.z},
        AxisAlignedPlane{SignedAxis(2, -1.0), bounds.max.z},
    };
}

} // namespace geometry
