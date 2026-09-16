#include <catch2/catch_test_macros.hpp>

#include <system_error>

#include <fmt/format.h>

#include "io/SerdeError.h"

TEST_CASE("A serde error formats what its code means for serialization") {
    CHECK(fmt::format("{}", io::SerdeError(std::errc::result_out_of_range)) == "the data ended early");
}
