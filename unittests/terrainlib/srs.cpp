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
#include "srs/transform_bounds.h"
#include "srs/transform_points.h"
#include "srs/transformation.h"

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

TEST_CASE("transformation fails between spatial references of different celestial bodies", "[srs]") {
    const OGRSpatialReference earth = REQUIRE_VALUE(srs::from_user_input("EPSG:4326"));
    const OGRSpatialReference mars = REQUIRE_VALUE(srs::from_user_input("IAU_2015:49900"));

    const alp::Error<srs::MissingTransformation> error = REQUIRE_ERROR(srs::transformation(earth, mars));

    const srs::MissingTransformation *missing = error.get_if<srs::MissingTransformation>();
    REQUIRE(missing != nullptr);
    CHECK(missing->source == "EPSG:4326");
    CHECK(missing->target == "Mars (2015) - Sphere / Ocentric");
}

// The orthographic projection onto the north pole maps no point south of the equator.
TEST_CASE("transforming fails for points outside the projection domain", "[srs]") {
    const OGRSpatialReference geographic = REQUIRE_VALUE(srs::from_user_input("EPSG:4326"));
    const OGRSpatialReference orthographic = REQUIRE_VALUE(srs::from_user_input("ESRI:102035"));
    const std::shared_ptr<OGRCoordinateTransformation> transform = REQUIRE_VALUE(srs::transformation(geographic, orthographic));

    std::optional<alp::Error<srs::UntransformablePoint>> error_opt;
    SECTION("transform_point in 2d") {
        error_opt = REQUIRE_ERROR(srs::transform_point(transform.get(), glm::dvec2(10, -20)));
    }
    SECTION("transform_point in 3d") {
        error_opt = REQUIRE_ERROR(srs::transform_point(transform.get(), glm::dvec3(10, -20, 300)));
    }
    SECTION("transform_points_inplace in 2d") {
        std::vector<glm::dvec2> points = {{10, 20}, {30, -40}, {50, 60}};
        error_opt = REQUIRE_ERROR(srs::transform_points_inplace(transform.get(), points));
    }
    SECTION("transform_points_inplace in 3d") {
        std::vector<glm::dvec3> points = {{10, 20, 300}, {30, -40, 500}, {50, 60, 700}};
        error_opt = REQUIRE_ERROR(srs::transform_points_inplace(transform.get(), points));
    }
    SECTION("transform_points_to_2d") {
        const std::vector<glm::dvec3> points = {{10, 20, 300}, {30, -40, 500}, {50, 60, 700}};
        error_opt = REQUIRE_ERROR(srs::transform_points_to_2d(transform.get(), points));
    }
    SECTION("non_exact_bounds_transform in 2d") {
        const radix::tile::SrsBounds bounds({10, -40}, {30, 20});
        error_opt = REQUIRE_ERROR(srs::non_exact_bounds_transform(transform.get(), bounds));
    }
    SECTION("non_exact_bounds_transform in 3d") {
        const radix::geometry::Aabb3d bounds({10, -40, 300}, {30, 20, 500});
        error_opt = REQUIRE_ERROR(srs::non_exact_bounds_transform(transform.get(), bounds));
    }
    SECTION("encompassing_bounds_transfer in 2d") {
        const radix::tile::SrsBounds bounds({10, -40}, {30, -20});
        error_opt = REQUIRE_ERROR(srs::encompassing_bounds_transfer(transform.get(), bounds));
    }
    SECTION("encompassing_bounds_transfer in 3d") {
        const radix::geometry::Aabb3d bounds({10, -40, 300}, {30, -20, 500});
        error_opt = REQUIRE_ERROR(srs::encompassing_bounds_transfer(transform.get(), bounds));
    }

    const srs::UntransformablePoint *untransformable = error_opt.value().get_if<srs::UntransformablePoint>();
    REQUIRE(untransformable != nullptr);
    CHECK(untransformable->source == "EPSG:4326");
    CHECK(untransformable->target == "North_Pole_Orthographic");
}
