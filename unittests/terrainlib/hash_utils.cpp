#include <cstddef>
#include <cstdint>
#include <string>

#include "../catch2_helpers.h"

#include "hash_utils.h"

TEST_CASE("combine returns the same hash for the same values", "[hash_utils]") {
    const size_t first = hash::combine(10, 20, 30);
    const size_t second = hash::combine(10, 20, 30);

    CHECK(first == second);
}

TEST_CASE("combine depends on the order of the values", "[hash_utils]") {
    CHECK(hash::combine(10, 20, 30) != hash::combine(30, 20, 10));
}

TEST_CASE("combine gives different hashes for different values", "[hash_utils]") {
    CHECK(hash::combine(10) != hash::combine(20));
    CHECK(hash::combine(10, 20) != hash::combine(30, 40));
}

TEST_CASE("append with several values matches appending them one at a time", "[hash_utils]") {
    const int first = 10;
    const uint32_t second = 20;
    const std::string third = "thirty";

    size_t together = hash::default_seed();
    hash::append(together, first, second, third);

    size_t separately = hash::default_seed();
    hash::append(separately, first);
    hash::append(separately, second);
    hash::append(separately, third);

    CHECK(together == separately);
}
