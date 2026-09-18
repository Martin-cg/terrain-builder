#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>

#include "../temp_directory.h"
#include "gdal/CplError.h"
#include "mask.h"

TEST_CASE("Loading a missing mask fails", "[sf_merger][mask]") {
    const TempDirectory directory("mask_missing");

    const auto result = mask::load_referenced_from_path(directory.path() / "missing.geojson");

    REQUIRE(!result.has_value());
    CHECK(result.error().holds<gdal::CplError>());
}

TEST_CASE("Loading a mask without polygons fails", "[sf_merger][mask]") {
    const TempDirectory directory("mask_without_polygons");
    const std::filesystem::path path = directory.path() / "point.geojson";
    std::ofstream(path) << R"({"type": "FeatureCollection", "features": [{"type": "Feature", "properties": {}, "geometry": {"type": "Point", "coordinates": [11.0, 47.0]}}]})";

    const auto result = mask::load_referenced_from_path(path);

    REQUIRE(!result.has_value());
    CHECK(result.error().holds(mask::ContentError::NoPolygons));
}
