#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <fmt/format.h>
#include <glm/glm.hpp>
#include <ogr_spatialref.h>
#include <radix/geometry.h>
#include <radix/tile.h>

#include "../result_helpers.h"
#include "error/format.h"
#include "gdal/OgrError.h"
#include "srs.h"
#include "srs/transform.h"
#include "srs/transform_bounds.h"

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

TEST_CASE("get_transform fails between spatial references of different celestial bodies", "[srs]") {
    const OGRSpatialReference earth = REQUIRE_VALUE(srs::from_user_input("EPSG:4326"));
    const OGRSpatialReference mars = REQUIRE_VALUE(srs::from_user_input("IAU_2015:49900"));

    const alp::Error<srs::MissingTransform> error = REQUIRE_ERROR(srs::get_transform(earth, mars));

    const srs::MissingTransform *missing = error.get_if<srs::MissingTransform>();
    REQUIRE(missing != nullptr);
    CHECK(missing->source == "EPSG:4326");
    CHECK(missing->target == "Mars (2015) - Sphere / Ocentric");
}

// The orthographic projection onto the north pole maps no point south of the equator.
TEST_CASE("transforming fails for points outside the projection domain", "[srs]") {
    const OGRSpatialReference geographic = REQUIRE_VALUE(srs::from_user_input("EPSG:4326"));
    const OGRSpatialReference orthographic = REQUIRE_VALUE(srs::from_user_input("ESRI:102035"));
    const srs::Transform transform = REQUIRE_VALUE(srs::get_transform(geographic, orthographic));

    std::optional<alp::Error<srs::Untransformable>> error_opt;
    SECTION("apply in 2d") {
        error_opt = REQUIRE_ERROR(transform.apply(glm::dvec2(10, -20)));
    }
    SECTION("apply in 3d") {
        error_opt = REQUIRE_ERROR(transform.apply(glm::dvec3(10, -20, 300)));
    }
    SECTION("apply_inplace in 2d") {
        std::vector<glm::dvec2> points = {{10, 20}, {30, -40}, {50, 60}};
        error_opt = REQUIRE_ERROR(transform.apply_inplace(points));
    }
    SECTION("apply_inplace in 3d") {
        std::vector<glm::dvec3> points = {{10, 20, 300}, {30, -40, 500}, {50, 60, 700}};
        error_opt = REQUIRE_ERROR(transform.apply_inplace(points));
    }
    SECTION("apply_into from 3d to 2d") {
        const std::vector<glm::dvec3> points = {{10, 20, 300}, {30, -40, 500}, {50, 60, 700}};
        std::vector<glm::dvec2> out(points.size());
        error_opt = REQUIRE_ERROR(transform.apply_into(points, out));
    }
    SECTION("non_exact_bounds_transform in 2d") {
        const radix::tile::SrsBounds bounds({10, -40}, {30, 20});
        error_opt = REQUIRE_ERROR(srs::non_exact_bounds_transform(transform, bounds));
    }
    SECTION("non_exact_bounds_transform in 3d") {
        const radix::geometry::Aabb3d bounds({10, -40, 300}, {30, 20, 500});
        error_opt = REQUIRE_ERROR(srs::non_exact_bounds_transform(transform, bounds));
    }
    SECTION("encompassing_bounds_transfer in 2d") {
        const radix::tile::SrsBounds bounds({10, -40}, {30, -20});
        error_opt = REQUIRE_ERROR(srs::encompassing_bounds_transfer(transform, bounds));
    }
    SECTION("encompassing_bounds_transfer in 3d") {
        const radix::geometry::Aabb3d bounds({10, -40, 300}, {30, -20, 500});
        error_opt = REQUIRE_ERROR(srs::encompassing_bounds_transfer(transform, bounds));
    }

    const srs::Untransformable *untransformable = error_opt.value().get_if<srs::Untransformable>();
    REQUIRE(untransformable != nullptr);
    CHECK(untransformable->source == "EPSG:4326");
    CHECK(untransformable->target == "North_Pole_Orthographic");
}

TEST_CASE("apply succeeds on an empty range", "[srs]") {
    const OGRSpatialReference geographic = REQUIRE_VALUE(srs::from_user_input("EPSG:4326"));
    const OGRSpatialReference mercator = REQUIRE_VALUE(srs::from_user_input("EPSG:3857"));
    const srs::Transform transform = REQUIRE_VALUE(srs::get_transform(geographic, mercator));

    SECTION("apply_inplace") {
        std::vector<glm::dvec2> points;
        REQUIRE_VALUE(transform.apply_inplace(points));
        CHECK(points.empty());
    }
    SECTION("apply_into") {
        const std::vector<glm::dvec3> points;
        std::vector<glm::dvec2> out;
        REQUIRE_VALUE(transform.apply_into(points, out));
        CHECK(out.empty());
    }
}
