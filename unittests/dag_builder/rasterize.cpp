#include <algorithm>
#include <cstdint>
#include <utility>
#include <vector>

#include <glm/glm.hpp>

#include "../catch2_helpers.h"

#include "mesh/WindingOrder.h"
#include "range_utils.h"
#include "rasterize.h"

namespace {

std::vector<glm::uvec2> scan(const raster::FixedTriangle &triangle, const glm::uvec2 grid_size) {
    std::vector<glm::uvec2> cells;
    raster::scan_triangle(triangle, grid_size, [&](const glm::uvec2 cell) {
        cells.push_back(cell);
    });
    std::ranges::sort(cells, {}, [](const glm::uvec2 c) { return std::pair(c.y, c.x); });
    return cells;
}

// Collects every cell of the grid whose index pair satisfies the predicate, in scan order.
template <typename Predicate>
std::vector<glm::uvec2> collect_cells(const glm::uvec2 grid_size, Predicate &&predicate) {
    std::vector<glm::uvec2> cells;
    for (const uint32_t y : range(grid_size.y)) {
        for (const uint32_t x : range(grid_size.x)) {
            if (predicate(x, y)) {
                cells.push_back(glm::uvec2(x, y));
            }
        }
    }
    return cells;
}

constexpr glm::uvec2 grid(4, 4);

} // namespace

TEST_CASE("scan_triangle visits the cells whose centre lies in the triangle", "[rasterize]") {
    // Half the grid, cut by the hypotenuse through (4,0) and (0,4).
    const raster::FixedTriangle triangle = raster::make_triangle({0, 0}, {4, 0}, {0, 4});

    // A centre sits at x + 0.5, so the cells on x + y == 3 land exactly on the hypotenuse and are kept.
    const std::vector<glm::uvec2> expected = collect_cells(grid, [](const uint32_t x, const uint32_t y) {
        return x + y <= 3;
    });

    CHECK(scan(triangle, grid) == expected);
}

TEST_CASE("scan_triangle visits the same cells for either winding", "[rasterize]") {
    const raster::FixedTriangle triangle = raster::make_triangle({0.5, 0.25}, {3.5, 1.0}, {1.0, 3.5});

    CHECK(scan(triangle, grid) == scan(swap_orientation(triangle), grid));
    CHECK_FALSE(scan(triangle, grid).empty());
}

TEST_CASE("scan_triangle covers nothing for a degenerate triangle", "[rasterize]") {
    SECTION("collinear corners") {
        CHECK(scan(raster::make_triangle({0, 0}, {2, 2}, {4, 4}), grid).empty());
    }
    SECTION("a single point") {
        CHECK(scan(raster::make_triangle({1.5, 1.5}, {1.5, 1.5}, {1.5, 1.5}), grid).empty());
    }
}

TEST_CASE("scan_triangle clamps to the grid", "[rasterize]") {
    // Far larger than the grid and hanging off both ends of it.
    const raster::FixedTriangle triangle = raster::make_triangle({-10, -10}, {20, -10}, {-10, 20});

    const std::vector<glm::uvec2> expected = collect_cells(grid, [](uint32_t, uint32_t) {
        return true;
    });

    CHECK(scan(triangle, grid) == expected);
}

TEST_CASE("scan_triangle visits a centre on a shared edge from both sides", "[rasterize]") {
    // The unit quad split along the diagonal through (0,0) and (4,4).
    const std::vector<glm::uvec2> lower = scan(raster::make_triangle({0, 0}, {4, 0}, {4, 4}), grid);
    const std::vector<glm::uvec2> upper = scan(raster::make_triangle({0, 0}, {4, 4}, {0, 4}), grid);

    // The diagonal runs through the centres of the cells on x == y, so neither half gives them up.
    const std::vector<glm::uvec2> diagonal = collect_cells(grid, [](const uint32_t x, const uint32_t y) {
        return x == y;
    });
    for (const glm::uvec2 cell : diagonal) {
        REQUIRE(std::ranges::find(lower, cell) != lower.end());
        REQUIRE(std::ranges::find(upper, cell) != upper.end());
    }

    CHECK(lower.size() + upper.size() == grid.x * grid.y + diagonal.size());
}
