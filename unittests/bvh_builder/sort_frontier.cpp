#include "../catch2_helpers.h"
#include "../temp_directory.h"

#include <algorithm>
#include <vector>

#include "range_utils.h"

#include "frontier/sort.h"
#include "frontier/Storage.h"
#include "frontier/traverse.h"

namespace {

constexpr uint32_t prefix_bits = 3;
constexpr uint32_t cell_count = 8;

// A code inside the given prefix cell.
morton::Code code_in_cell(const uint32_t cell, const uint32_t offset = 0) {
    return (morton::Code{cell} << (morton::code_bits - prefix_bits)) + morton::Code{offset};
}

frontier::Entry entry_with(const morton::Code &code) {
    return frontier::Entry{
        .morton = code,
        .header = bvh::NodeHeader{
            .min_child_error = 1.0,
            .max_parent_error = 2.0,
            .triangle_count = 3,
            .bounds = bvh::Bounds{glm::dvec3{0.0}, glm::dvec3{1.0}},
        },
        .ref = bvh::Reference{.batch = 0, .node = 0},
    };
}

// `entries_per_batch[i]` entries in batch i, taken in order from `codes`.
void write_run(frontier::Storage &run, const std::vector<morton::Code> &codes, const std::vector<uint32_t> &entries_per_batch) {
    size_t next = 0;
    for (const uint32_t count : entries_per_batch) {
        frontier::Batch batch;
        for (uint32_t i = 0; i < count; i++) {
            batch.entries.push_back(entry_with(codes[next]));
            next++;
        }
        REQUIRE(run.write(batch).has_value());
    }
    REQUIRE(next == codes.size());
    REQUIRE(run.save_index().has_value());
}

std::vector<morton::Code> read_codes(const frontier::Storage &run) {
    std::vector<morton::Code> codes;
    const auto result = frontier::traverse_entries(run, [&](const frontier::Entry &entry) { codes.push_back(entry.morton); });
    REQUIRE(result.has_value());
    return codes;
}

// One code per entry, `counts[cell]` of them in each cell.
std::vector<morton::Code> codes_per_cell(const std::vector<uint32_t> &counts) {
    std::vector<morton::Code> codes;
    for (const auto &[cell, count] : enumerate<uint32_t>(counts)) {
        for (const uint32_t i : range(count)) {
            codes.push_back(code_in_cell(cell, i));
        }
    }
    return codes;
}

morton::PrefixMap<uint64_t> histogram_of(const std::vector<uint64_t> &counts) {
    morton::PrefixMap<uint64_t> histogram(prefix_bits, 0);
    for (const auto &[cell, count] : enumerate<uint32_t>(counts)) {
        histogram.at(code_in_cell(cell)) = count;
    }
    return histogram;
}

}

TEST_CASE("a prefix map indexes by the leading bits of the code", "[sort_frontier]") {
    morton::PrefixMap<uint32_t> map(prefix_bits, 0);

    REQUIRE(map.cells().size() == cell_count);
    CHECK(map.prefix_bits() == prefix_bits);

    map.at(code_in_cell(5)) = 42;
    CHECK(map.at(code_in_cell(5, 1234)) == 42);
    CHECK(map.at(code_in_cell(4)) == 0);
    CHECK(map.cells()[5] == 42);
}

TEST_CASE("sort_entries orders by morton", "[sort_frontier]") {
    std::vector<frontier::Entry> entries = {
        entry_with(code_in_cell(6)),
        entry_with(code_in_cell(1)),
        entry_with(code_in_cell(3)),
    };

    frontier::sort_entries(entries);

    CHECK(entries[0].morton == code_in_cell(1));
    CHECK(entries[1].morton == code_in_cell(3));
    CHECK(entries[2].morton == code_in_cell(6));
}

TEST_CASE("sort_batches_independently leaves every entry in its own batch", "[sort_frontier]") {
    const TempDirectory directory("sort_frontier");
    frontier::Storage run(directory.path());
    write_run(run, {code_in_cell(4), code_in_cell(1), code_in_cell(7), code_in_cell(2)}, {2, 2});

    REQUIRE(frontier::sort_batches_independently(run).has_value());

    CHECK(read_codes(run) == std::vector<morton::Code>{code_in_cell(1), code_in_cell(4), code_in_cell(2), code_in_cell(7)});
}

TEST_CASE("build_histogram counts the entries of every cell", "[sort_frontier]") {
    const TempDirectory directory("sort_frontier");
    frontier::Storage input(directory.path());
    write_run(input, codes_per_cell({2, 0, 0, 1, 0, 3, 0, 0}), {3, 3});

    const auto histogram = frontier::build_histogram(input, prefix_bits);
    REQUIRE(histogram.has_value());

    CHECK(histogram.value().prefix_bits() == prefix_bits);
    CHECK(std::ranges::equal(histogram.value().cells(), std::vector<uint64_t>{2, 0, 0, 1, 0, 3, 0, 0}));
}

TEST_CASE("assign_buckets fills buckets up to the limit", "[sort_frontier]") {
    const frontier::Buckets buckets = frontier::assign_buckets(histogram_of({1, 1, 1, 1, 1, 1, 1, 1}), 3);

    // Three cells fill a bucket, so the seventh cell opens the third one.
    CHECK(std::ranges::equal(buckets.assignment.cells(), std::vector<uint32_t>{0, 0, 0, 1, 1, 1, 2, 2}));
    CHECK(buckets.count == 3);
    CHECK(buckets.assignment.prefix_bits() == prefix_bits);
}

TEST_CASE("assign_buckets does not spend a bucket on empty cells", "[sort_frontier]") {
    const frontier::Buckets buckets = frontier::assign_buckets(histogram_of({2, 0, 0, 0, 0, 0, 0, 2}), 2);

    // The empty run between the two populated cells must not become a bucket of its own.
    CHECK(buckets.count == 2);
    CHECK(buckets.assignment.at(code_in_cell(0)) == 0);
    CHECK(buckets.assignment.at(code_in_cell(7)) == 1);
    CHECK(std::ranges::is_sorted(buckets.assignment.cells()));
}

TEST_CASE("assign_buckets keeps an oversized cell in one bucket", "[sort_frontier]") {
    const frontier::Buckets buckets = frontier::assign_buckets(histogram_of({0, 0, 10, 0, 0, 0, 0, 0}), 2);

    // Nothing splits a cell, so an oversized one still maps to a single bucket.
    CHECK(buckets.count == 1);
    CHECK(buckets.assignment.at(code_in_cell(2)) == buckets.assignment.at(code_in_cell(2, 99)));
}

TEST_CASE("scatter writes one batch per bucket in morton order", "[sort_frontier]") {
    const TempDirectory directory("sort_frontier");
    std::filesystem::create_directories(directory.path() / "in");
    std::filesystem::create_directories(directory.path() / "out");

    frontier::Storage input(directory.path() / "in");
    write_run(input, codes_per_cell({2, 0, 0, 2, 0, 2, 0, 0}), {3, 3});

    const auto histogram = frontier::build_histogram(input, prefix_bits);
    REQUIRE(histogram.has_value());
    const frontier::Buckets buckets = frontier::assign_buckets(histogram.value(), 2);

    frontier::Storage output(directory.path() / "out");
    REQUIRE(frontier::scatter(input, buckets, output, 8).has_value());
    REQUIRE(output.save_index().has_value());

    REQUIRE(output.size() == 3);
    for (const uint32_t index : range(output.size())) {
        const auto batch = output.read(index);
        REQUIRE(batch.has_value());
        for (const frontier::Entry &entry : batch.value().entries) {
            REQUIRE(buckets.assignment.at(entry.morton) == index);
        }
    }
}

TEST_CASE("scatter over more buckets than it may hold open takes several passes", "[sort_frontier]") {
    const TempDirectory directory("sort_frontier");
    std::filesystem::create_directories(directory.path() / "in");
    std::filesystem::create_directories(directory.path() / "wide");
    std::filesystem::create_directories(directory.path() / "narrow");

    frontier::Storage input(directory.path() / "in");
    write_run(input, codes_per_cell({2, 2, 2, 2, 2, 2, 2, 2}), {8, 8});

    const auto histogram = frontier::build_histogram(input, prefix_bits);
    REQUIRE(histogram.has_value());
    const frontier::Buckets buckets = frontier::assign_buckets(histogram.value(), 2);

    frontier::Storage wide(directory.path() / "wide");
    REQUIRE(frontier::scatter(input, buckets, wide, 8).has_value());

    frontier::Storage narrow(directory.path() / "narrow");
    REQUIRE(frontier::scatter(input, buckets, narrow, 1).has_value());

    CHECK(wide.size() == narrow.size());
    CHECK(read_codes(wide) == read_codes(narrow));
}

TEST_CASE("sort_by_morton orders the whole run", "[sort_frontier]") {
    const TempDirectory directory("sort_frontier");
    std::filesystem::create_directories(directory.path() / "in");
    std::filesystem::create_directories(directory.path() / "out");

    const std::vector<morton::Code> codes = {
        code_in_cell(5), code_in_cell(0, 7), code_in_cell(5, 1), code_in_cell(2), code_in_cell(0), code_in_cell(7),
    };

    frontier::Storage input(directory.path() / "in");
    write_run(input, codes, {4, 2});

    frontier::Storage output(directory.path() / "out");
    const frontier::SortOptions options{
        .max_batch_bytes = 2 * sizeof(frontier::Entry),
        .max_resident_bytes = 4 * sizeof(frontier::Entry),
        .prefix_bits = prefix_bits,
    };
    REQUIRE(frontier::sort_by_morton(input, output, options).has_value());
    REQUIRE(output.save_index().has_value());

    std::vector<morton::Code> expected = codes;
    std::ranges::sort(expected);
    CHECK(read_codes(output) == expected);
}

TEST_CASE("sort_by_morton handles an empty run", "[sort_frontier]") {
    const TempDirectory directory("sort_frontier");
    std::filesystem::create_directories(directory.path() / "in");
    std::filesystem::create_directories(directory.path() / "out");

    const frontier::Storage input(directory.path() / "in");
    frontier::Storage output(directory.path() / "out");
    const frontier::SortOptions options{.prefix_bits = prefix_bits};

    REQUIRE(frontier::sort_by_morton(input, output, options).has_value());
    CHECK(output.size() == 0);
}
