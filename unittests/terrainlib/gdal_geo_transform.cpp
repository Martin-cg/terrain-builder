#include <array>
#include <memory>

#include <gdal_priv.h>
#include <glm/glm.hpp>

#include "../catch2_helpers.h"
#include "../result_helpers.h"
#include "gdal/GeoTransform.h"
#include "init.h"

namespace {

struct DatasetCloser {
    void operator()(GDALDataset *dataset) const {
        GDALClose(dataset);
    }
};

// An in-memory raster of 4 by 4 pixels without a geotransform.
std::unique_ptr<GDALDataset, DatasetCloser> make_dataset() {
    initialize_gdal_once();
    GDALDriver *driver = GetGDALDriverManager()->GetDriverByName("MEM");
    REQUIRE(driver != nullptr);
    return std::unique_ptr<GDALDataset, DatasetCloser>(driver->Create("", 4, 4, 1, GDT_Float32, nullptr));
}

} // namespace

TEST_CASE("A geotransform maps a pixel into the srs and back", "[gdal][GeoTransform]") {
    const gdal::GeoTransform transform = REQUIRE_VALUE(gdal::GeoTransform::from_forward({10, 2, 0, 50, 0, -4}));

    CHECK(transform.apply(glm::dvec2(0, 0)) == glm::dvec2(10, 50));
    CHECK(transform.apply(glm::dvec2(3, 1)) == glm::dvec2(16, 46));
    CHECK(transform.apply_inverse(glm::dvec2(16, 46)) == glm::dvec2(3, 1));
}

TEST_CASE("A geotransform with zero sized pixels cannot be created", "[gdal][GeoTransform]") {
    const alp::Error<gdal::GeoTransformError> error = REQUIRE_ERROR(gdal::GeoTransform::from_forward({10, 0, 0, 50, 0, 0}));

    CHECK(error.holds(gdal::GeoTransformError::NotInvertible));
}

TEST_CASE("A geotransform is axis aligned unless it shears or rotates", "[gdal][GeoTransform]") {
    const gdal::GeoTransform aligned = REQUIRE_VALUE(gdal::GeoTransform::from_forward({10, 2, 0, 50, 0, -4}));
    const gdal::GeoTransform sheared = REQUIRE_VALUE(gdal::GeoTransform::from_forward({10, 2, 1, 50, 0, -4}));

    CHECK(aligned.is_axis_aligned());
    CHECK(!sheared.is_axis_aligned());
}

TEST_CASE("Reading the geotransform of a dataset yields the one that was set", "[gdal][GeoTransform]") {
    const std::unique_ptr<GDALDataset, DatasetCloser> dataset = make_dataset();
    std::array<double, 6> forward = {10, 2, 0, 50, 0, -4};
    REQUIRE(dataset->SetGeoTransform(forward.data()) == CE_None);

    const gdal::GeoTransform transform = REQUIRE_VALUE(gdal::GeoTransform::from_dataset(*dataset));

    CHECK(transform == REQUIRE_VALUE(gdal::GeoTransform::from_forward(forward)));
}

TEST_CASE("Reading the geotransform of a dataset without one fails as missing", "[gdal][GeoTransform]") {
    const std::unique_ptr<GDALDataset, DatasetCloser> dataset = make_dataset();

    const alp::Error<gdal::GeoTransformError> error = REQUIRE_ERROR(gdal::GeoTransform::from_dataset(*dataset));

    CHECK(error.holds(gdal::GeoTransformError::Missing));
}
