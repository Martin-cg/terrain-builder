#pragma once

#include <cstddef>
#include <ranges>
#include <tuple>
#include <type_traits>
#include <utility>

namespace detail {

template <typename TIndex, typename TValue>
struct enumerate_item {
    TIndex index;
    TValue value;
};

} // namespace detail

template <typename TIndex = std::size_t, std::ranges::viewable_range Range>
auto enumerate(Range &&range) {
    return std::views::enumerate(std::forward<Range>(range))
        | std::views::transform([](const auto &pair) {
              using Value = std::tuple_element_t<1, std::remove_cvref_t<decltype(pair)>>;
              return detail::enumerate_item<TIndex, Value>{static_cast<TIndex>(std::get<0>(pair)), std::get<1>(pair)};
          });
}