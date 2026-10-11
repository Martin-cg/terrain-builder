#include "../catch2_helpers.h"

#include <cstdint>

#include "range_utils.h"

#include "morton/utils.h"

namespace {

const morton::Code top_bit = morton::Code{1} << (morton::code_bits - 1);

}

TEST_CASE("the common prefix of two codes counts the leading bits they share", "[morton][utils]") {
    const morton::Code zero{0};

    CHECK(morton::count_common_prefix_bits(zero, zero) == morton::code_bits);
    CHECK(morton::count_common_prefix_bits(zero, top_bit) == 0);
    CHECK(morton::count_common_prefix_bits(zero, morton::Code{1}) == morton::code_bits - 1);

    for (const uint32_t bit : range(morton::code_bits)) {
        const morton::Code differing = morton::Code{1} << bit;
        REQUIRE(morton::count_common_prefix_bits(zero, differing) == morton::code_bits - 1 - bit);
    }
}
