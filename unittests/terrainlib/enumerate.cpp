#include "../catch2_helpers.h"
#include "enumerate.h"

#include <cstdint>
#include <ranges>
#include <type_traits>
#include <utility>
#include <vector>

TEST_CASE("enumerate vector of ints", "[enumerate]") {
    std::vector<int> vec = {10, 20, 30};
    std::vector<std::size_t> indices;
    std::vector<int> values;
    for (auto item : enumerate(vec)) {
        indices.push_back(item.index);
        values.push_back(item.value);
    }
    CHECK(indices == std::vector<std::size_t>{0, 1, 2});
    CHECK(values == std::vector<int>{10, 20, 30});
}

TEST_CASE("enumerate with mutation", "[enumerate]") {
    std::vector<int> vec = {1, 2, 3};
    for (auto &&[index, value] : enumerate(vec)) {
        value *= 10;
    }
    CHECK(vec == std::vector<int>{10, 20, 30});
}

TEST_CASE("enumerate const vector", "[enumerate]") {
    const std::vector<int> vec = {5, 6, 7};
    STATIC_REQUIRE(std::is_same_v<decltype(std::declval<std::ranges::range_reference_t<decltype(enumerate(vec))>>().value), const int &>);

    std::vector<int> values;
    for (const auto &[index, value] : enumerate(vec)) {
        values.push_back(value);
    }
    CHECK(values == std::vector<int>{5, 6, 7});
}

TEST_CASE("enumerate with custom index type", "[enumerate]") {
    const std::vector<int> vec = {100, 200};
    STATIC_REQUIRE(std::is_same_v<decltype(std::declval<std::ranges::range_reference_t<decltype(enumerate<uint32_t>(vec))>>().index), uint32_t>);

    std::vector<uint32_t> indices;
    for (const auto &[index, value] : enumerate<uint32_t>(vec)) {
        indices.push_back(index);
    }
    CHECK(indices == std::vector<uint32_t>{0, 1});
}