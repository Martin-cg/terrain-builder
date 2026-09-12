#include "../catch2_helpers.h"
#include "containers/OffsetTable.h"

#include <array>
#include <cstdint>
#include <ranges>
#include <vector>

TEST_CASE("OffsetTable default construction") {
    OffsetTable table;
    CHECK(table.empty());
    CHECK(table.size() == 0);
    CHECK(table.total_size() == 0);
}

TEST_CASE("OffsetTable append_segment") {
    OffsetTable table;
    table.append_segment(3);
    table.append_segment(5);
    table.append_segment(2);

    CHECK(table.size() == 3);
    CHECK(table.total_size() == 10);

    auto r0 = table.segment_range(0);
    CHECK(r0.begin == 0);
    CHECK(r0.end == 3);

    auto r1 = table.segment_range(1);
    CHECK(r1.begin == 3);
    CHECK(r1.end == 8);

    auto r2 = table.segment_range(2);
    CHECK(r2.begin == 8);
    CHECK(r2.end == 10);
}

TEST_CASE("OffsetTable append_segments via span") {
    OffsetTable table;
    std::array<size_t, 3> lengths = {3, 5, 2};
    table.append_segments(lengths);

    CHECK(table.size() == 3);
    CHECK(table.total_size() == 10);

    auto r0 = table.segment_range(0);
    CHECK(r0.begin == 0);
    CHECK(r0.end == 3);

    auto r1 = table.segment_range(1);
    CHECK(r1.begin == 3);
    CHECK(r1.end == 8);

    auto r2 = table.segment_range(2);
    CHECK(r2.begin == 8);
    CHECK(r2.end == 10);
}

TEST_CASE("OffsetTable element_size") {
    OffsetTable table;
    table.append_segment(3);
    table.append_segment(5);
    table.append_segment(2);

    CHECK(table.segment_size(0) == 3);
    CHECK(table.segment_size(1) == 5);
    CHECK(table.segment_size(2) == 2);
}

TEST_CASE("OffsetTable locate") {
    OffsetTable table;
    table.append_segment(3);
    table.append_segment(5);
    table.append_segment(2);

    {
        auto result = table.locate(0);
        CHECK(result.segment == 0);
        CHECK(result.range.begin == 0);
        CHECK(result.range.end == 3);
    }
    {
        auto result = table.locate(3);
        CHECK(result.segment == 1);
        CHECK(result.range.begin == 3);
        CHECK(result.range.end == 8);
    }
    {
        auto result = table.locate(7);
        CHECK(result.segment == 1);
        CHECK(result.range.begin == 3);
        CHECK(result.range.end == 8);
    }
    {
        auto result = table.locate(8);
        CHECK(result.segment == 2);
        CHECK(result.range.begin == 8);
        CHECK(result.range.end == 10);
    }
    {
        auto result = table.locate(9);
        CHECK(result.segment == 2);
        CHECK(result.range.begin == 8);
        CHECK(result.range.end == 10);
    }
}

TEST_CASE("OffsetTable locate out of range throws") {
    OffsetTable table;
    table.append_segment(3);
    table.append_segment(5);
    table.append_segment(2);

    CHECK_THROWS_AS(table.locate(10), std::out_of_range);
    CHECK_THROWS_AS(table.locate(100), std::out_of_range);
}

TEST_CASE("OffsetTable range out of range throws") {
    OffsetTable table;
    table.append_segment(3);
    table.append_segment(5);

    CHECK_THROWS_AS(table.segment_range(2), std::out_of_range);
    CHECK_THROWS_AS(table.segment_range(100), std::out_of_range);
}

TEST_CASE("OffsetTable set_end on last element to extend") {
    OffsetTable table;
    table.append_segment(3);
    table.append_segment(5);
    table.append_segment(2);

    table.set_end(2, 15);

    CHECK(table.total_size() == 15);
    auto r2 = table.segment_range(2);
    CHECK(r2.begin == 8);
    CHECK(r2.end == 15);
}

TEST_CASE("OffsetTable set_begin on non-first element") {
    OffsetTable table;
    table.append_segment(3);
    table.append_segment(5);
    table.append_segment(2);

    table.set_begin(1, 4);

    auto r0 = table.segment_range(0);
    CHECK(r0.begin == 0);
    CHECK(r0.end == 4);

    auto r1 = table.segment_range(1);
    CHECK(r1.begin == 4);
    CHECK(r1.end == 8);
}

TEST_CASE("OffsetTable set_begin on element 0 throws") {
    OffsetTable table;
    table.append_segment(3);
    table.append_segment(5);

    CHECK_THROWS_AS(table.set_begin(0, 1), std::out_of_range);
}

TEST_CASE("OffsetTable clear then re-add elements") {
    OffsetTable table;
    table.append_segment(3);
    table.append_segment(5);

    CHECK(table.size() == 2);
    CHECK(table.total_size() == 8);

    table.clear();

    CHECK(table.empty());
    CHECK(table.size() == 0);
    CHECK(table.total_size() == 0);

    table.append_segment(4);
    table.append_segment(6);

    CHECK(table.size() == 2);
    CHECK(table.total_size() == 10);

    auto r0 = table.segment_range(0);
    CHECK(r0.begin == 0);
    CHECK(r0.end == 4);

    auto r1 = table.segment_range(1);
    CHECK(r1.begin == 4);
    CHECK(r1.end == 10);
}

TEST_CASE("OffsetTable pop_segment drops the last segment") {
    OffsetTable table;

    table.append_segment(3);
    table.append_segment(5);

    table.pop_segment();

    REQUIRE(table.size() == 1);
    CHECK(table.total_size() == 3);
    CHECK(table.segment_range(0).end == 3);
}

TEST_CASE("OffsetTable pop_segments drops several at once") {
    OffsetTable table;

    table.append_segment(3);
    table.append_segment(5);
    table.append_segment(7);

    table.pop_segments(2);

    REQUIRE(table.size() == 1);
    CHECK(table.total_size() == 3);
}

TEST_CASE("OffsetTable pop_segments can empty the table") {
    OffsetTable table;

    table.append_segment(3);
    table.append_segment(5);

    table.pop_segments(2);

    CHECK(table.empty());
    CHECK(table.size() == 0);
    CHECK(table.total_size() == 0);
}

TEST_CASE("OffsetTable pop_segments rejects more than it holds") {
    OffsetTable table;

    table.append_segment(3);

    CHECK_THROWS_AS(table.pop_segments(2), std::out_of_range);
    CHECK(table.size() == 1);
}

TEST_CASE("OffsetTable appends after popping continue from the shortened end") {
    OffsetTable table;

    table.append_segment(3);
    table.append_segment(5);

    table.pop_segment();
    table.append_segment(2);

    REQUIRE(table.size() == 2);
    CHECK(table.total_size() == 5);

    const auto range = table.segment_range(1);
    CHECK(range.begin == 3);
    CHECK(range.end == 5);
}

TEST_CASE("build_offset_table fills a table up to the limit of its index type") {
    const std::array<uint8_t, 3> lengths = {1, 0, 254};

    const OffsetTable_<uint8_t> table = build_offset_table<uint8_t>(lengths);

    REQUIRE(table.size() == 3);
    CHECK(table.total_size() == 255);

    const auto r0 = table.segment_range(0);
    CHECK(r0.begin == 0);
    CHECK(r0.end == 1);

    const auto r1 = table.segment_range(1);
    CHECK(r1.begin == 1);
    CHECK(r1.end == 1);

    const auto r2 = table.segment_range(2);
    CHECK(r2.begin == 1);
    CHECK(r2.end == 255);
}

TEST_CASE("build_offset_table totals narrow lengths in a wider index type") {
    const std::array<uint8_t, 2> lengths = {200, 100};

    const OffsetTable table = build_offset_table(lengths);

    REQUIRE(table.size() == 2);
    CHECK(table.total_size() == 300);
    CHECK(table.segment_range(1).begin == 200);
}

TEST_CASE("build_offset_table takes lengths from a projected range") {
    struct Item {
        uint8_t count;
    };
    const std::array<Item, 3> items = {Item{3}, Item{5}, Item{2}};

    const OffsetTable table = build_offset_table(items | std::views::transform(&Item::count));

    REQUIRE(table.size() == 3);
    CHECK(table.total_size() == 10);
    CHECK(table.segment_range(1).begin == 3);
    CHECK(table.segment_range(2).begin == 8);
}

TEST_CASE("build_offset_table of no lengths is empty") {
    const std::array<uint8_t, 0> lengths = {};

    const OffsetTable table = build_offset_table(lengths);

    CHECK(table.empty());
    CHECK(table.total_size() == 0);
}
