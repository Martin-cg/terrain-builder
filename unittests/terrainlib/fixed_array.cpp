#include <cstdint>
#include <utility>
#include <vector>

#include "../catch2_helpers.h"

#include "containers/FixedArray.h"

TEST_CASE("FixedArray value-initialises its elements", "[containers][FixedArray]") {
    const FixedArray<uint32_t> array(4);

    REQUIRE(array.size() == 4);
    for (const uint32_t value : array) {
        REQUIRE(value == 0);
    }
}

TEST_CASE("FixedArray fills every element with the given value", "[containers][FixedArray]") {
    const FixedArray<uint32_t> array(3, 7u);

    REQUIRE(array.size() == 3);
    for (const uint32_t value : array) {
        REQUIRE(value == 7);
    }
}

TEST_CASE("FixedArray holds the values of an initializer list", "[containers][FixedArray]") {
    const FixedArray<uint32_t> array = {1, 2, 3};

    CHECK(std::vector<uint32_t>(array.begin(), array.end()) == std::vector<uint32_t>{1, 2, 3});
}

TEST_CASE("FixedArray compares equal when size and elements match", "[containers][FixedArray]") {
    const FixedArray<uint32_t> array = {1, 2, 3};

    CHECK(array == FixedArray<uint32_t>{1, 2, 3});
    CHECK_FALSE(array == FixedArray<uint32_t>{1, 2, 4});
    CHECK_FALSE(array == FixedArray<uint32_t>{1, 2});
}

TEST_CASE("FixedArray of size zero owns no storage", "[containers][FixedArray]") {
    const FixedArray<uint32_t> array(0);

    CHECK(array.empty());
    CHECK(array.data() == nullptr);
    CHECK(array.begin() == array.end());
}

TEST_CASE("FixedArray copies are deep", "[containers][FixedArray]") {
    const FixedArray<uint32_t> source = {1, 2, 3};

    SECTION("construction") {
        const FixedArray<uint32_t> copy(source);

        CHECK(copy.data() != source.data());
        CHECK(copy == FixedArray<uint32_t>{1, 2, 3});
    }
    SECTION("assignment") {
        FixedArray<uint32_t> copy(1);
        copy = source;

        CHECK(copy.data() != source.data());
        CHECK(copy == FixedArray<uint32_t>{1, 2, 3});
    }
}

TEST_CASE("FixedArray moves hand over the storage and leave the source empty", "[containers][FixedArray]") {
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

TEST_CASE("FixedArray survives assignment to itself", "[containers][FixedArray]") {
    FixedArray<uint32_t> array = {1, 2, 3};
    FixedArray<uint32_t> &alias = array;

    SECTION("copy") {
        array = alias;
    }
    SECTION("move") {
        array = std::move(alias);
    }

    CHECK(array == FixedArray<uint32_t>{1, 2, 3});
}

TEST_CASE("FixedArray of bool stores one plain bool per element", "[containers][FixedArray]") {
    FixedArray<bool> flags(3);
    flags[1] = true;

    const bool *raw = flags.data();
    CHECK(!raw[0]);
    CHECK(raw[1]);
    CHECK(!raw[2]);
}