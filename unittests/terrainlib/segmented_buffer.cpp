#include <cstdint>
#include <utility>
#include <vector>

#include "../catch2_helpers.h"
#include "containers/SegmentedBuffer.h"

TEST_CASE("SegmentedBuffer default construction", "[containers][SegmentedBuffer]") {
    SegmentedBuffer<int> buf;

    CHECK(buf.segment_count() == 0);
    CHECK(buf.total_size() == 0);
}

TEST_CASE("SegmentedBuffer has no last segment while empty", "[containers][SegmentedBuffer]") {
    SegmentedBuffer<int> buf;

    CHECK(buf.last_segment().empty());
}

TEST_CASE("SegmentedBuffer opens a segment for the first push", "[containers][SegmentedBuffer]") {
    SegmentedBuffer<int> buf;

    buf.push_to_last_segment(1);

    REQUIRE(buf.segment_count() == 1);
    CHECK(buf.segment_size(0) == 1);
}

TEST_CASE("SegmentedBuffer construct from vector", "[containers][SegmentedBuffer]") {
    std::vector<int> data = {10, 20, 30};
    SegmentedBuffer<int> buf(data);

    REQUIRE(buf.segment_count() == 1);
    CHECK(buf.total_size() == 3);
    REQUIRE(buf.segment_size(0) == 3);
    CHECK(buf(0, 0) == 10);
    CHECK(buf(0, 1) == 20);
    CHECK(buf(0, 2) == 30);
}

TEST_CASE("SegmentedBuffer construct from vector and offsets", "[containers][SegmentedBuffer]") {
    OffsetTable offsets;
    offsets.append_lengths(std::vector<size_t>{2, 0, 3});

    const SegmentedBuffer<int> buf(std::vector<int>{10, 20, 30, 40, 50}, std::move(offsets));

    SegmentedBuffer<int> expected;
    expected.push_new_segment(std::vector<int>{10, 20});
    expected.push_new_segment(std::vector<int>{});
    expected.push_new_segment(std::vector<int>{30, 40, 50});
    CHECK(buf == expected);
}

TEST_CASE("SegmentedBuffer construct from empty vector and offsets", "[containers][SegmentedBuffer]") {
    const SegmentedBuffer<int> buf(std::vector<int>{}, OffsetTable{});

    CHECK(buf.segment_count() == 0);
    CHECK(buf.total_size() == 0);
}

TEST_CASE("SegmentedBuffer push_to_last_segment", "[containers][SegmentedBuffer]") {
    SegmentedBuffer<int> buf;

    buf.push_to_last_segment(1);
    buf.push_to_last_segment(2);
    buf.push_to_last_segment(3);

    REQUIRE(buf.segment_count() == 1);
    REQUIRE(buf.segment_size(0) == 3);
    CHECK(buf(0, 0) == 1);
    CHECK(buf(0, 1) == 2);
    CHECK(buf(0, 2) == 3);
}

TEST_CASE("SegmentedBuffer start_new_segment then push_to_last_segment", "[containers][SegmentedBuffer]") {
    SegmentedBuffer<int> buf;

    buf.push_to_last_segment(10);
    buf.push_to_last_segment(20);

    buf.start_new_segment();

    buf.push_to_last_segment(30);
    buf.push_to_last_segment(40);
    buf.push_to_last_segment(50);

    REQUIRE(buf.segment_count() == 2);
    REQUIRE(buf.segment_size(0) == 2);
    REQUIRE(buf.segment_size(1) == 3);
    CHECK(buf.total_size() == 5);
    CHECK(buf(0, 0) == 10);
    CHECK(buf(0, 1) == 20);
    CHECK(buf(1, 0) == 30);
    CHECK(buf(1, 1) == 40);
    CHECK(buf(1, 2) == 50);
}

TEST_CASE("SegmentedBuffer push_new_segment with size", "[containers][SegmentedBuffer]") {
    SegmentedBuffer<int> buf;

    buf.push_new_segment(4, 99);

    REQUIRE(buf.segment_count() == 1);
    REQUIRE(buf.segment_size(0) == 4);
    CHECK(buf.total_size() == 4);
    CHECK(buf(0, 0) == 99);
    CHECK(buf(0, 1) == 99);
    CHECK(buf(0, 2) == 99);
    CHECK(buf(0, 3) == 99);
}

TEST_CASE("SegmentedBuffer resize_last_segment after start_new_segment", "[containers][SegmentedBuffer]") {
    SegmentedBuffer<int> buf;

    buf.push_new_segment(2, 1);
    buf.push_new_segment(3, 2);

    REQUIRE(buf.segment_count() == 2);
    REQUIRE(buf.segment_size(0) == 2);
    REQUIRE(buf.segment_size(1) == 3);
    REQUIRE(buf.total_size() == 5);
    CHECK(buf(0, 0) == 1);
    CHECK(buf(0, 1) == 1);
    CHECK(buf(1, 0) == 2);
    CHECK(buf(1, 1) == 2);
    CHECK(buf(1, 2) == 2);
}

TEST_CASE("SegmentedBuffer push_new_segment from range", "[containers][SegmentedBuffer]") {
    SegmentedBuffer<int> buf;

    buf.push_to_last_segment(1);

    std::vector<int> data = {100, 200, 300};
    buf.push_new_segment(data);

    REQUIRE(buf.segment_count() == 2);
    REQUIRE(buf.segment_size(0) == 1);
    REQUIRE(buf.segment_size(1) == 3);
    REQUIRE(buf.total_size() == 4);
    CHECK(buf(0, 0) == 1);
    CHECK(buf(1, 0) == 100);
    CHECK(buf(1, 1) == 200);
    CHECK(buf(1, 2) == 300);
}

TEST_CASE("SegmentedBuffer init with segment sizes", "[containers][SegmentedBuffer]") {
    SegmentedBuffer<int> buf;

    std::vector<size_t> sizes = {2, 3, 1};
    buf.init(sizes, 42);

    REQUIRE(buf.segment_count() == 3);
    REQUIRE(buf.segment_size(0) == 2);
    REQUIRE(buf.segment_size(1) == 3);
    REQUIRE(buf.segment_size(2) == 1);
    REQUIRE(buf.total_size() == 6);
    CHECK(buf(0, 0) == 42);
    CHECK(buf(1, 2) == 42);
    CHECK(buf(2, 0) == 42);
}

TEST_CASE("SegmentedBuffer operator() access across segments", "[containers][SegmentedBuffer]") {
    SegmentedBuffer<int> buf;

    std::vector<size_t> sizes = {2, 3};
    buf.init(sizes, 0);

    buf(0, 0) = 10;
    buf(0, 1) = 20;
    buf(1, 0) = 30;
    buf(1, 1) = 40;
    buf(1, 2) = 50;

    CHECK(buf(0, 0) == 10);
    CHECK(buf(0, 1) == 20);
    CHECK(buf(1, 0) == 30);
    CHECK(buf(1, 1) == 40);
    CHECK(buf(1, 2) == 50);
}

TEST_CASE("SegmentedBuffer segment() span view", "[containers][SegmentedBuffer]") {
    SegmentedBuffer<int> buf;

    buf.push_to_last_segment(5);
    buf.push_to_last_segment(6);

    buf.start_new_segment();

    buf.push_to_last_segment(7);

    auto seg0 = buf.segment(0);
    auto seg1 = buf.segment(1);

    REQUIRE(seg0.size() == 2);
    CHECK(seg0[0] == 5);
    CHECK(seg0[1] == 6);

    REQUIRE(seg1.size() == 1);
    CHECK(seg1[0] == 7);
}

TEST_CASE("SegmentedBuffer flat() view", "[containers][SegmentedBuffer]") {
    SegmentedBuffer<int> buf;

    buf.push_to_last_segment(1);
    buf.push_to_last_segment(2);

    buf.start_new_segment();

    buf.push_to_last_segment(3);
    buf.push_to_last_segment(4);
    buf.push_to_last_segment(5);

    auto flat = buf.flat();

    REQUIRE(flat.size() == 5);
    CHECK(flat[0] == 1);
    CHECK(flat[1] == 2);
    CHECK(flat[2] == 3);
    CHECK(flat[3] == 4);
    CHECK(flat[4] == 5);
}

TEST_CASE("SegmentedBuffer reset", "[containers][SegmentedBuffer]") {
    SegmentedBuffer<int> buf;

    buf.push_to_last_segment(1);
    buf.push_to_last_segment(2);
    buf.start_new_segment();
    buf.push_to_last_segment(3);

    REQUIRE(buf.segment_count() == 2);
    REQUIRE(buf.total_size() == 3);

    buf.reset();

    REQUIRE(buf.segment_count() == 0);
    REQUIRE(buf.total_size() == 0);
}

TEST_CASE("SegmentedBuffer multiple segments with different sizes and last_segment", "[containers][SegmentedBuffer]") {
    SegmentedBuffer<int> buf;

    buf.push_to_last_segment(10);

    buf.start_new_segment();
    buf.push_to_last_segment(20);
    buf.push_to_last_segment(21);

    buf.start_new_segment();
    buf.push_to_last_segment(30);
    buf.push_to_last_segment(31);
    buf.push_to_last_segment(32);

    REQUIRE(buf.segment_count() == 3);
    REQUIRE(buf.segment_size(0) == 1);
    REQUIRE(buf.segment_size(1) == 2);
    REQUIRE(buf.segment_size(2) == 3);
    REQUIRE(buf.total_size() == 6);

    auto last = buf.last_segment();
    REQUIRE(last.size() == 3);
    CHECK(last[0] == 30);
    CHECK(last[1] == 31);
    CHECK(last[2] == 32);
}

// A caller starting a segment per item must get one segment per item, even where an item
// contributes nothing. The leading placeholder segment used to swallow the second start.
TEST_CASE("SegmentedBuffer keeps empty segments", "[containers][SegmentedBuffer]") {
    SegmentedBuffer<int> buf;

    buf.start_new_segment(); // first item, contributes nothing
    buf.start_new_segment(); // second item
    buf.push_to_last_segment(10);
    buf.start_new_segment(); // third item, contributes nothing
    buf.start_new_segment(); // fourth item
    buf.push_to_last_segment(20);

    REQUIRE(buf.segment_count() == 4);
    CHECK(buf.segment_size(0) == 0);
    CHECK(buf.segment_size(1) == 1);
    CHECK(buf.segment_size(2) == 0);
    CHECK(buf.segment_size(3) == 1);
    CHECK(buf.total_size() == 2);
}

TEST_CASE("SegmentedBuffer keeps empty segments after reset", "[containers][SegmentedBuffer]") {
    SegmentedBuffer<int> buf;
    buf.start_new_segment();
    buf.push_to_last_segment(1);
    buf.reset();

    buf.start_new_segment();
    buf.start_new_segment();

    REQUIRE(buf.segment_count() == 2);
    CHECK(buf.segment_size(0) == 0);
    CHECK(buf.segment_size(1) == 0);
}

// init and the vector constructor install real segments, so a later start_new_segment
// must append rather than adopt one of them.
TEST_CASE("SegmentedBuffer start_new_segment after init appends", "[containers][SegmentedBuffer]") {
    SegmentedBuffer<int> buf;
    const std::vector<size_t> sizes{2, 3};
    buf.init(sizes);

    buf.start_new_segment();
    buf.push_to_last_segment(9);

    REQUIRE(buf.segment_count() == 3);
    CHECK(buf.segment_size(2) == 1);
}

TEST_CASE("SegmentedBuffer start_new_segment after the vector constructor appends", "[containers][SegmentedBuffer]") {
    SegmentedBuffer<int> buf(std::vector<int>{1, 2, 3});

    buf.start_new_segment();
    buf.push_to_last_segment(9);

    REQUIRE(buf.segment_count() == 2);
    CHECK(buf.segment_size(0) == 3);
    CHECK(buf.segment_size(1) == 1);
}

TEST_CASE("SegmentedBuffer start_new_segment after the vector and offsets constructor appends", "[containers][SegmentedBuffer]") {
    OffsetTable offsets;
    offsets.append_lengths(std::vector<size_t>{1, 2});
    SegmentedBuffer<int> buf(std::vector<int>{1, 2, 3}, std::move(offsets));

    buf.start_new_segment();
    buf.push_to_last_segment(9);

    REQUIRE(buf.segment_count() == 3);
    CHECK(buf.segment_size(0) == 1);
    CHECK(buf.segment_size(1) == 2);
    CHECK(buf.segment_size(2) == 1);
}

TEST_CASE("group_by_key keeps pair order per key and an empty segment for an unused key", "[containers][SegmentedBuffer]") {
    const std::vector<std::pair<uint32_t, uint32_t>> pairs = {
        {2, 30},
        {0, 11},
        {2, 12},
        {3, 7},
        {0, 13},
        {2, 14},
    };

    const SegmentedBuffer<uint32_t, uint32_t> groups = group_by_key<uint32_t, uint32_t>(uint32_t{4}, pairs);

    SegmentedBuffer<uint32_t, uint32_t> expected;
    expected.push_new_segment(std::vector<uint32_t>{11, 13});
    expected.push_new_segment(std::vector<uint32_t>{});
    expected.push_new_segment(std::vector<uint32_t>{30, 12, 14});
    expected.push_new_segment(std::vector<uint32_t>{7});
    CHECK(groups == expected);
}

TEST_CASE("group_by_key without pairs gives empty segments", "[containers][SegmentedBuffer]") {
    const std::vector<std::pair<uint32_t, uint32_t>> pairs;

    const SegmentedBuffer<uint32_t, uint32_t> groups = group_by_key<uint32_t, uint32_t>(uint32_t{2}, pairs);

    SegmentedBuffer<uint32_t, uint32_t> expected;
    expected.push_new_segment(std::vector<uint32_t>{});
    expected.push_new_segment(std::vector<uint32_t>{});
    CHECK(groups == expected);
}
