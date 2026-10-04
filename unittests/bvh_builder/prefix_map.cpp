#include "../catch2_helpers.h"

#include <cstdint>

#include "morton/PrefixMap.h"
#include "morton/code.h"

namespace {

constexpr uint32_t prefix_bits = 3;
constexpr uint32_t cell_count = 8;

// A code inside the given prefix cell.
morton::Code make_code_in_cell(const uint32_t cell, const uint32_t offset = 0) {
    return (morton::Code{cell} << (morton::code_bits - prefix_bits)) + morton::Code{offset};
}

}

TEST_CASE("a prefix map indexes by the leading bits of the code", "[morton][PrefixMap]") {
    morton::PrefixMap<uint32_t> map(prefix_bits, 0);

    REQUIRE(map.cells().size() == cell_count);
    CHECK(map.prefix_bits() == prefix_bits);

    map.at(make_code_in_cell(5)) = 42;
    CHECK(map.at(make_code_in_cell(5, 1234)) == 42);
    CHECK(map.at(make_code_in_cell(4)) == 0);
    CHECK(map.cells()[5] == 42);
}
