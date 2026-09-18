#pragma once

#include <bit>
#include <cstdint>
#include <limits>

#include <glm/glm.hpp>
#include <glm/vector_relational.hpp>
#include <libassert/assert.hpp>
#include <radix/geometry.h>

#include "glm_utils.h"
#include "numeric/int_math.h"

namespace packer {

// A conversion between points and integer coordinates.
template <glm::length_t D, typename T>
struct PointCodec {
    using Point = glm::vec<D, T>;
    using Coordinate = glm::vec<D, uint64_t>;

    // The point at coordinate zero.
    Point anchor = {};
    // The coordinate spacing on each axis is two to this power, in the units of a point.
    glm::vec<D, uint8_t> exponent = {};

    bool operator==(const PointCodec &) const = default;

    // The coordinate for `p`, rounded in `Rounding`'s direction.
    template <std::float_round_style Rounding = std::round_to_nearest>
    constexpr Coordinate encode(const Point p) const {
        const Point offset = p - this->anchor;
        DEBUG_ASSERT(glm::all(glm::greaterThanEqual(offset, Point{})));

        return transform(offset, this->exponent, [](const T o, const uint8_t e) {
            return static_cast<uint64_t>(int_div_by_style<Rounding>(o, T{1} << e));
        });
    }

    // The point at coordinate `c`.
    constexpr Point decode(const Coordinate c) const {
        const Point offset = transform(c, this->exponent, [](const uint64_t v, const uint8_t e) {
            return static_cast<T>(v << e);
        });
        return this->anchor + offset;
    }
};

// The codec addressing `bounds` with coordinates of `bits` bits.
template <glm::length_t D, typename T>
[[nodiscard]] PointCodec<D, T> fit_codec(const radix::geometry::Aabb<D, T> &bounds, const uint8_t bits) {
    DEBUG_ASSERT(bits > 0);

    const uint64_t coordinates = max_for_bits(bits);
    const glm::vec<D, T> size = bounds.max - bounds.min;

    const glm::vec<D, uint8_t> exponent = transform(size, [coordinates](const T extent) {
        DEBUG_ASSERT(extent >= 0);

        // Each coordinate must cover at least this many units, or the far corner is unreachable.
        const uint64_t minimum_step = int_div_ceil(static_cast<uint64_t>(extent), coordinates);

        // A degenerate axis needs no step at all, and the subtraction below would wrap.
        if (minimum_step == 0) {
            return uint8_t{0};
        }
        return static_cast<uint8_t>(std::bit_width(minimum_step - 1));
    });

    return {bounds.min, exponent};
}

}
