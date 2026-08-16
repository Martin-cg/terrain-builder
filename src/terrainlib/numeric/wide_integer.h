#pragma once

// Must precede the uintwide_t.h include: it is only honoured on the header's first
// inclusion in a translation unit, so nothing else may include it first.
#if defined(__SIZEOF_INT128__)
#define WIDE_INTEGER_HAS_LIMB_TYPE_UINT64
#endif
#include <math/wide_integer/uintwide_t.h>

#include <cstdint>

#if defined(__SIZEOF_INT128__)
using wide_limb_t = std::uint64_t;
#else
using wide_limb_t = std::uint32_t;
#endif

#include <fmt/ostream.h>
template <auto Width, typename LimbType, typename AllocatorType, bool IsSigned>
struct fmt::formatter<::math::wide_integer::uintwide_t<Width, LimbType, AllocatorType, IsSigned>> : fmt::ostream_formatter {};

#include <bit>
#include <limits>
#include <type_traits>
#include <utility>

#include <zpp_bits.h>

namespace math::wide_integer {

namespace detail {
template <typename Archive>
constexpr bool endian_aware() {
    if constexpr (requires { std::remove_cvref_t<Archive>::endian_aware; }) {
        return std::remove_cvref_t<Archive>::endian_aware;
    } else {
        return false;
    }
}

template <typename Archive>
inline constexpr bool wire_is_little = (std::endian::native == std::endian::little) != endian_aware<Archive>();

template <typename Archive, typename Limbs, std::size_t... Index>
constexpr auto serialize_limbs(Archive &archive, Limbs &limbs, std::index_sequence<Index...>) {
    if constexpr (wire_is_little<Archive>) {
        return archive(limbs[Index]...);
    } else {
        return archive(limbs[sizeof...(Index) - 1 - Index]...);
    }
}
}

template <typename Archive, size_t Width, typename LimbType, typename AllocatorType, bool IsSigned>
constexpr auto serialize(Archive &archive, const uintwide_t<Width, LimbType, AllocatorType, IsSigned> &value) {
    constexpr std::size_t limb_count = Width / std::numeric_limits<LimbType>::digits;
    return detail::serialize_limbs(archive, value.crepresentation(), std::make_index_sequence<limb_count>{});
}

template <typename Archive, size_t Width, typename LimbType, typename AllocatorType, bool IsSigned>
constexpr auto serialize(Archive &archive, uintwide_t<Width, LimbType, AllocatorType, IsSigned> &value) {
    constexpr std::size_t limb_count = Width / std::numeric_limits<LimbType>::digits;
    return detail::serialize_limbs(archive, value.representation(), std::make_index_sequence<limb_count>{});
}

}
