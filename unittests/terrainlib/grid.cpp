#include <optional>
#include <stdexcept>

#include <radix/tile.h>

#include "../catch2_helpers.h"

#include "ctb/GlobalGeodetic.hpp"
#include "ctb/GlobalMercator.hpp"

// Web Mercator spans [-20037508.34, 20037508.34] on both axes, with a single root tile.
// Geodetic spans [-180, 180] x [-90, 90], with two root tiles side by side.

TEST_CASE("Grid::findSmallestEncompassingTile stops where the next level would split the bounds", "[ctb][Grid]") {
    const ctb::GlobalMercator grid;
    // Zoom 2 tile (1, 2) spans x [-10018754.17, 0] and y [0, 10018754.17].
    // The bounds cross its centre lines at x = -5009377.09 and y = 5009377.09, so no zoom 3 tile holds them.
    const radix::tile::SrsBounds bounds{{-6e6, 4e6}, {-4e6, 6e6}};

    const std::optional<radix::tile::Id> tile = grid.findSmallestEncompassingTile(bounds);

    REQUIRE(tile.has_value());
    CHECK(*tile == radix::tile::Id{2, {1, 2}, radix::tile::Scheme::Tms});
}

TEST_CASE("Grid::findSmallestEncompassingTile descends from the root tile holding the bounds", "[ctb][Grid]") {
    const ctb::GlobalGeodetic grid;
    // Inside the eastern root tile. Zoom 1 tile (2, 1) spans [0, 90] x [0, 90].
    // The bounds cross y = 45, the boundary between its zoom 2 children.
    const radix::tile::SrsBounds bounds{{10.0, 40.0}, {20.0, 50.0}};

    const std::optional<radix::tile::Id> tile = grid.findSmallestEncompassingTile(bounds);

    REQUIRE(tile.has_value());
    CHECK(*tile == radix::tile::Id{1, {2, 1}, radix::tile::Scheme::Tms});
}

TEST_CASE("Grid::findSmallestEncompassingTile stops at the deepest zoom level the grid supports", "[ctb][Grid]") {
    const ctb::GlobalMercator grid;
    // Zoom 23 tile (4194304, 4194304) spans [0, 4.777] on both axes, and every deeper tile holding the bounds starts at 0 as well.
    const radix::tile::SrsBounds bounds{{2.0, 2.0}, {2.1, 2.1}};

    const std::optional<radix::tile::Id> tile = grid.findSmallestEncompassingTile(bounds);

    REQUIRE(tile.has_value());
    CHECK(*tile == radix::tile::Id{23, {4194304, 4194304}, radix::tile::Scheme::Tms});
}

TEST_CASE("Grid::findSmallestEncompassingTile finds nothing for bounds outside the grid", "[ctb][Grid]") {
    const ctb::GlobalMercator grid;
    const radix::tile::SrsBounds bounds{{3e7, 0.0}, {3.1e7, 1e6}};

    CHECK_FALSE(grid.findSmallestEncompassingTile(bounds).has_value());
}

TEST_CASE("Grid::findSmallestEncompassingTile rejects empty bounds", "[ctb][Grid]") {
    const ctb::GlobalMercator grid;
    const radix::tile::SrsBounds bounds{{1.0, 0.0}, {1.0, 5.0}};

    CHECK_THROWS_AS(grid.findSmallestEncompassingTile(bounds), std::invalid_argument);
}

TEST_CASE("Grid::getMaxZoomLevel is the deepest level whose pixel coordinates fit", "[ctb][Grid]") {
    // A 256 pixel tile uses 8 of the 32 bits, and the border pixel rules out the level that would fill all of them.
    CHECK(ctb::GlobalMercator(256).getMaxZoomLevel() == 23);
    // Two root tiles side by side double the pixel columns, which costs one level.
    CHECK(ctb::GlobalGeodetic(256).getMaxZoomLevel() == 22);
}

TEST_CASE("Grid::srsBounds covers the tile in either scheme", "[ctb][Grid]") {
    const ctb::GlobalMercator grid;
    // The north-western zoom 1 tile, which is (0, 1) counted from the south and (0, 0) from the north.
    const radix::tile::Id tile = GENERATE(
        radix::tile::Id{1, {0, 1}, radix::tile::Scheme::Tms},
        radix::tile::Id{1, {0, 0}, radix::tile::Scheme::SlippyMap});

    const radix::tile::SrsBounds bounds = grid.srsBounds(tile, false);

    CHECK_THAT(bounds.min.x, WithinEpsilon(-20037508.342789244));
    CHECK(bounds.min.y == 0.0);
    CHECK(bounds.max.x == 0.0);
    CHECK_THAT(bounds.max.y, WithinEpsilon(20037508.342789244));
}

TEST_CASE("Grid::srsBounds extends the maximum corner by one pixel with a border", "[ctb][Grid]") {
    const ctb::GlobalMercator grid;
    const radix::tile::Id tile{1, {0, 1}, radix::tile::Scheme::Tms};

    const radix::tile::SrsBounds bounds = grid.srsBounds(tile, true);

    // A zoom 1 pixel is 40075016.69 / 512 = 78271.52 wide.
    CHECK_THAT(bounds.min.x, WithinEpsilon(-20037508.342789244));
    CHECK(bounds.min.y == 0.0);
    CHECK_THAT(bounds.max.x, WithinEpsilon(78271.51696402048));
    CHECK_THAT(bounds.max.y, WithinEpsilon(20037508.342789244 + 78271.51696402048));
}

TEST_CASE("Grid::crsToTile finds the tile holding the point", "[ctb][Grid]") {
    const ctb::GlobalMercator grid;

    // Zoom 2 tile (1, 2) spans x [-10018754.17, 0] and y [0, 10018754.17].
    CHECK(grid.crsToTile({-5e6, 5e6}, 2) == radix::tile::Id{2, {1, 2}, radix::tile::Scheme::Tms});
}