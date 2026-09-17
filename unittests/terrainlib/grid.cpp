/*****************************************************************************
 * Alpine Terrain Builder
 * Copyright (C) 2022 alpinemaps.org
 * Copyright (C) 2022 Adam Celarek <family name at cg tuwien ac at>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *****************************************************************************/

#include <optional>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <radix/tile.h>

#include "ctb/GlobalGeodetic.hpp"
#include "ctb/GlobalMercator.hpp"

// word of warning:
// the Grid class is taken from ctb, which didn't use unit tests.
// Therefore it's unlikely, that it will be fully covered by tests.
// New functions should be tested though!

using Catch::Approx;

TEST_CASE("grid", "[ctb][GlobalGeodetic][GlobalMercator]") {
    SECTION("epsg codes") {
        const auto geodetic = ctb::GlobalGeodetic(256);
        CHECK(geodetic.getEpsgCode() == 4326);

        const auto webmercator = ctb::GlobalMercator(256);
        CHECK(webmercator.getEpsgCode() == 3857);

        CHECK(!geodetic.getSRS().IsSame(&webmercator.getSRS()));
    }

    SECTION("bounds") {
        const auto geodetic = ctb::GlobalGeodetic(256);
        CHECK(geodetic.getExtent().min.x == Approx(-180.0));
        CHECK(geodetic.getExtent().max.x == Approx(180.0));
        CHECK(geodetic.getExtent().min.y == Approx(-90.0));
        CHECK(geodetic.getExtent().max.y == Approx(90.0));

        const auto webmercator = ctb::GlobalMercator(256);
        const auto lim = double(20037508);
        CHECK(webmercator.getExtent().min.x == Approx(-lim));
        CHECK(webmercator.getExtent().max.x == Approx(lim));
        CHECK(webmercator.getExtent().min.y == Approx(-lim));
        CHECK(webmercator.getExtent().max.y == Approx(lim));
    }
}

TEST_CASE("Grid::findSmallestEncompassingTile stops at the deepest zoom level the grid supports", "[ctb][Grid]") {
    const ctb::GlobalMercator grid;
    // Zoom 23 tile (4194304, 4194304) spans [0, 4.777] on both axes, and every deeper tile holding the bounds starts at 0 as well.
    const radix::tile::SrsBounds bounds{{2.0, 2.0}, {2.1, 2.1}};

    const std::optional<radix::tile::Id> tile = grid.findSmallestEncompassingTile(bounds);

    REQUIRE(tile.has_value());
    CHECK(*tile == radix::tile::Id{23, {4194304, 4194304}, radix::tile::Scheme::Tms});
}

TEST_CASE("Grid::getMaxZoomLevel is the deepest level whose pixel coordinates fit", "[ctb][Grid]") {
    // A 256 pixel tile uses 8 of the 32 bits, and the border pixel rules out the level that would fill all of them.
    CHECK(ctb::GlobalMercator(256).getMaxZoomLevel() == 23);
    // Two root tiles side by side double the pixel columns, which costs one level.
    CHECK(ctb::GlobalGeodetic(256).getMaxZoomLevel() == 22);
}
