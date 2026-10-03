#include <catch2/catch_test_macros.hpp>

#include <string>

#include <fmt/format.h>

#include "error/format.h"
#include "gdal/OgrError.h"
#include "srs.h"

TEST_CASE("Importing an unknown EPSG code fails", "[srs]") {
    const auto result = srs::from_epsg(999999);

    REQUIRE(!result.has_value());
    CHECK(result.error().holds<gdal::OgrError>());
}

TEST_CASE("Parsing invalid spatial reference input fails", "[srs]") {
    const std::string input = "not a spatial reference";

    const auto result = srs::from_user_input(input);

    REQUIRE(!result.has_value());
    CHECK(result.error().holds<gdal::OgrError>());
}
