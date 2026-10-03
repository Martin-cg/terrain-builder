#include <cstdint>
#include <optional>
#include <set>
#include <tuple>

#include "../catch2_helpers.h"

#include "containers/HybridIndexPairMap.h"

TEST_CASE("HybridIndexPairMap is empty when default constructed", "[containers][HybridIndexPairMap]") {
    const HybridIndexPairMap<uint32_t, uint32_t> map;

    CHECK(map.empty());
    CHECK(map.size() == 0);
    CHECK(map.find(0, 0) == std::nullopt);
    CHECK(map.find_ptr(0, 0) == nullptr);
    CHECK_FALSE(map.contains(0, 0));
}

TEST_CASE("HybridIndexPairMap insert_or_assign stores a value under a key pair", "[containers][HybridIndexPairMap]") {
    HybridIndexPairMap<uint32_t, uint32_t> map;

    map.insert_or_assign(3, 7, 42);

    CHECK_FALSE(map.empty());
    CHECK(map.size() == 1);
    CHECK(map.find(3, 7) == 42);
    CHECK(map.contains(3, 7));

    const uint32_t *value = map.find_ptr(3, 7);
    REQUIRE(value != nullptr);
    CHECK(*value == 42);
}

TEST_CASE("HybridIndexPairMap insert_or_assign replaces the value of a key pair", "[containers][HybridIndexPairMap]") {
    HybridIndexPairMap<uint32_t, uint32_t> map;
    map.insert_or_assign(5, 10, 100);

    map.insert_or_assign(5, 10, 200);

    CHECK(map.size() == 1);
    CHECK(map.find(5, 10) == 200);
}

TEST_CASE("HybridIndexPairMap insert_or_assign keeps three values under one primary key", "[containers][HybridIndexPairMap]") {
    HybridIndexPairMap<uint32_t, uint32_t> map;

    map.insert_or_assign(1, 10, 100);
    map.insert_or_assign(1, 20, 200);
    map.insert_or_assign(1, 30, 300);

    CHECK(map.size() == 3);
    CHECK(map.find(1, 10) == 100);
    CHECK(map.find(1, 20) == 200);
    CHECK(map.find(1, 30) == 300);
}

TEST_CASE("HybridIndexPairMap insert_or_assign replaces a value stored under a shared primary key", "[containers][HybridIndexPairMap]") {
    HybridIndexPairMap<uint32_t, uint32_t> map;
    map.insert_or_assign(0, 1, 10);
    map.insert_or_assign(0, 2, 20);

    map.insert_or_assign(0, 1, 99);

    CHECK(map.size() == 2);
    CHECK(map.find(0, 1) == 99);
    CHECK(map.find(0, 2) == 20);
}

TEST_CASE("HybridIndexPairMap find returns nothing for a key pair that was never inserted", "[containers][HybridIndexPairMap]") {
    HybridIndexPairMap<uint32_t, uint32_t> map;
    map.insert_or_assign(5, 10, 42);

    SECTION("unused primary key") {
        CHECK(map.find(99, 10) == std::nullopt);
        CHECK(map.find_ptr(99, 10) == nullptr);
        CHECK_FALSE(map.contains(99, 10));
    }

    SECTION("used primary key with an unused secondary key") {
        CHECK(map.find(5, 99) == std::nullopt);
        CHECK(map.find_ptr(5, 99) == nullptr);
        CHECK_FALSE(map.contains(5, 99));
    }

    SECTION("primary key holding several secondary keys") {
        map.insert_or_assign(5, 20, 43);

        CHECK(map.find(5, 99) == std::nullopt);
        CHECK(map.find_ptr(5, 99) == nullptr);
        CHECK_FALSE(map.contains(5, 99));
    }
}

TEST_CASE("HybridIndexPairMap clear empties the map", "[containers][HybridIndexPairMap]") {
    HybridIndexPairMap<uint32_t, uint32_t> map;
    map.insert_or_assign(1, 10, 100);
    map.insert_or_assign(2, 20, 200);
    map.insert_or_assign(2, 30, 300);

    map.clear();

    CHECK(map.empty());
    CHECK(map.size() == 0);
    CHECK(map.find(1, 10) == std::nullopt);
    CHECK(map.find(2, 20) == std::nullopt);
    CHECK_FALSE(map.contains(2, 30));

    map.insert_or_assign(4, 40, 400);

    CHECK(map.size() == 1);
    CHECK(map.find(4, 40) == 400);
}

TEST_CASE("HybridIndexPairMap entries lists every stored value", "[containers][HybridIndexPairMap]") {
    HybridIndexPairMap<uint32_t, uint32_t> map;
    map.insert_or_assign(0, 10, 100);
    map.insert_or_assign(1, 20, 200);
    map.insert_or_assign(2, 30, 300);
    map.insert_or_assign(2, 40, 400);

    std::set<std::tuple<uint32_t, uint32_t, uint32_t>> collected;
    for (const auto &entry : map.entries()) {
        collected.emplace(entry.primary_key, entry.secondary_key, entry.value);
    }

    const std::set<std::tuple<uint32_t, uint32_t, uint32_t>> expected{
        {0, 10, 100},
        {1, 20, 200},
        {2, 30, 300},
        {2, 40, 400},
    };
    CHECK(collected == expected);
}

TEST_CASE("HybridIndexPairMap insert_or_assign stores a value of a type other than the index", "[containers][HybridIndexPairMap]") {
    HybridIndexPairMap<uint32_t, double> map;
    map.insert_or_assign(1, 2, 3.14);

    SECTION("replacing the value under the same key pair") {
        map.insert_or_assign(1, 2, 2.72);

        CHECK(map.size() == 1);
        const std::optional<double> result = map.find(1, 2);
        REQUIRE(result.has_value());
        CHECK(*result == 2.72);
    }

    SECTION("adding a value under the same primary key") {
        map.insert_or_assign(1, 3, 2.72);

        CHECK(map.size() == 2);
        const std::optional<double> first_result = map.find(1, 2);
        REQUIRE(first_result.has_value());
        CHECK(*first_result == 3.14);

        const std::optional<double> second_result = map.find(1, 3);
        REQUIRE(second_result.has_value());
        CHECK(*second_result == 2.72);
    }
}
