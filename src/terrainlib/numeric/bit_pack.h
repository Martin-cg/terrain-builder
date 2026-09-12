#pragma once

#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>

// The `Wide` holding `parts` in order, the first in its most significant bits.
template <std::unsigned_integral Wide, std::unsigned_integral... Parts>
[[nodiscard]] constexpr Wide pack_ints(const Parts... parts) noexcept {
    static_assert((sizeof(Parts) + ...) == sizeof(Wide));

    Wide packed = 0;
    ((packed = static_cast<Wide>((packed << std::numeric_limits<Parts>::digits) | parts)), ...);
    return packed;
}

// The `Wide` holding `parts` in order, the first in its most significant bits.
template <std::unsigned_integral Wide, std::unsigned_integral Part, size_t N>
[[nodiscard]] constexpr Wide pack_ints(const std::array<Part, N> &parts) noexcept {
    static_assert(N * sizeof(Part) == sizeof(Wide));

    Wide packed = 0;
    for (const Part part : parts) {
        packed = static_cast<Wide>((packed << std::numeric_limits<Part>::digits) | part);
    }
    return packed;
}

// The parts of `value`, the most significant first.
template <std::unsigned_integral Part, std::unsigned_integral Wide>
[[nodiscard]] constexpr auto unpack_ints(const Wide value) noexcept {
    constexpr size_t count = sizeof(Wide) / sizeof(Part);
    static_assert(count * sizeof(Part) == sizeof(Wide));

    std::array<Part, count> parts = {};
    for (size_t index = 0; index < count; index++) {
        const size_t shift = std::numeric_limits<Part>::digits * (count - 1 - index);
        parts[index] = static_cast<Part>(value >> shift);
    }

    return parts;
}

static_assert(pack_ints<uint32_t>(uint16_t{0xabcd}, uint16_t{0x1234}) == 0xabcd1234);
static_assert(pack_ints<uint32_t>(std::array<uint8_t, 4>{0x12, 0x34, 0x56, 0x78}) == 0x12345678);
static_assert(unpack_ints<uint16_t>(uint32_t{0xabcd1234})[0] == 0xabcd);
static_assert(unpack_ints<uint16_t>(uint32_t{0xabcd1234})[1] == 0x1234);