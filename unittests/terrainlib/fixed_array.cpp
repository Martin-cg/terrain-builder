#include <cstdint>
#include <utility>

#include "../catch2_helpers.h"

#include "containers/FixedArray.h"

TEST_CASE("FixedArray value-initialises its elements", "[terrainlib][fixed_array]") {
    const FixedArray<uint32_t> array(4);

    REQUIRE(array.size() == 4);
    for (const uint32_t value : array) {
        REQUIRE(value == 0);
    }
}

TEST_CASE("FixedArray fills every element with the given value", "[terrainlib][fixed_array]") {
    const FixedArray<uint32_t> array(3, 7u);

    REQUIRE(array.size() == 3);
    for (const uint32_t value : array) {
        REQUIRE(value == 7);
    }
}

TEST_CASE("FixedArray of size zero owns no storage", "[terrainlib][fixed_array]") {
    const FixedArray<uint32_t> array(0);

    CHECK(array.empty());
    CHECK(array.data() == nullptr);
    CHECK(array.begin() == array.end());
}

TEST_CASE("FixedArray copies are deep", "[terrainlib][fixed_array]") {
    FixedArray<uint32_t> source(3, 5u);

    SECTION("construction") {
        FixedArray<uint32_t> copy(source);
        copy[0] = 9;

        CHECK(copy.size() == 3);
        CHECK(source[0] == 5);
    }
    SECTION("assignment") {
        FixedArray<uint32_t> copy(1);
        copy = source;
        copy[0] = 9;

        CHECK(copy.size() == 3);
        CHECK(copy[1] == 5);
        CHECK(source[0] == 5);
    }
}

TEST_CASE("FixedArray moves hand over the storage and leave the source empty", "[terrainlib][fixed_array]") {
    FixedArray<uint32_t> source(3, 5u);
    const uint32_t *storage = source.data();

    SECTION("construction") {
        const FixedArray<uint32_t> moved(std::move(source));

        CHECK(moved.data() == storage);
        CHECK(moved.size() == 3);
    }
    SECTION("assignment") {
        FixedArray<uint32_t> moved(1);
        moved = std::move(source);

        CHECK(moved.data() == storage);
        CHECK(moved.size() == 3);
    }

    CHECK(source.empty());
    CHECK(source.begin() == source.end());
}

TEST_CASE("FixedArray survives assignment to itself", "[terrainlib][fixed_array]") {
    FixedArray<uint32_t> array(3, 5u);
    FixedArray<uint32_t> &alias = array;

    SECTION("copy") {
        array = alias;
    }
    SECTION("move") {
        array = std::move(alias);
    }

    REQUIRE(array.size() == 3);
    CHECK(array[0] == 5);
    CHECK(array[2] == 5);
}

TEST_CASE("FixedArray of bool stores one plain bool per element", "[terrainlib][fixed_array]") {
    FixedArray<bool> flags(3);
    flags[1] = true;

    const bool *raw = flags.data();
    CHECK(!raw[0]);
    CHECK(raw[1]);
    CHECK(!raw[2]);
}