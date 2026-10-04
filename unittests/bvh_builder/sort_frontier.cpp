#include "../catch2_helpers.h"
#include "../result_helpers.h"
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
morton::Code make_code_in_cell(const uint32_t cell, const uint32_t offset = 0) {
    return (morton::Code{cell} << (morton::code_bits - prefix_bits)) + morton::Code{offset};
}

frontier::Entry make_entry(const morton::Code &code) {
    return frontier::Entry{
        .morton = code,
        .header = {},
        .ref = {},
    };
}

// `entries_per_batch[i]` entries in batch i, taken in order from `codes`.
void write_run(frontier::Storage &run, const std::vector<morton::Code> &codes, const std::vector<uint32_t> &entries_per_batch) {
    size_t next = 0;
    for (const uint32_t count : entries_per_batch) {
        REQUIRE(count <= codes.size() - next);

        frontier::Batch batch;
        for (uint32_t i = 0; i < count; i++) {
            batch.entries.push_back(make_entry(codes[next]));
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
std::vector<morton::Code> make_codes_per_cell(const std::vector<uint32_t> &counts) {
    std::vector<morton::Code> codes;
    for (const auto &[cell, count] : enumerate<uint32_t>(counts)) {
        for (const uint32_t i : range(count)) {
            codes.push_back(make_code_in_cell(cell, i));
        }
    }
    return codes;
}

morton::PrefixMap<uint64_t> make_histogram(const std::vector<uint64_t> &counts) {
    morton::PrefixMap<uint64_t> histogram(prefix_bits, 0);
    for (const auto &[cell, count] : enumerate<uint32_t>(counts)) {
        histogram.at(make_code_in_cell(cell)) = count;
    }
    return histogram;
}

}

TEST_CASE("a prefix map indexes by the leading bits of the code", "[morton][PrefixMap]") {
    morton::PrefixMap<uint32_t> map(prefix_bits, 0);

    REQUIRE(map.cells().size() == cell_count);
    CHECK(map.prefix_bits() == prefix_bits);

    map.at(make_code_in_cell(5)) = 42;
    CHECK(map.at(make_code_in_cell(5, 1234)) == 42);
    CHECK(map.at(make_code_in_cell(4)) == 0);
    CHECK(map.cells()[5] == 42);
}

TEST_CASE("sort_entries orders by morton", "[frontier][sort]") {
    const frontier::Entry a{.morton = 6, .header = {}, .ref = {.batch = 0, .node = 0}};
    const frontier::Entry b{.morton = 1, .header = {}, .ref = {.batch = 0, .node = 1}};
    const frontier::Entry c{.morton = 3, .header = {}, .ref = {.batch = 0, .node = 2}};

    std::vector<frontier::Entry> entries = {a, b, c};
    frontier::sort_entries(entries);

    CHECK(entries == std::vector<frontier::Entry>{b, c, a});
}

TEST_CASE("sort_entries orders equal codes by header, then by reference", "[frontier][sort]") {
    // The header decides although the references say the opposite, and the reference only between equal headers.
    const frontier::Entry a{.morton = 5, .header = {.min_child_error = 2.0, .max_parent_error = 4.0, .bounds = {}}, .ref = {.batch = 0, .node = 0}};
    const frontier::Entry b{.morton = 5, .header = {.min_child_error = 1.0, .max_parent_error = 4.0, .bounds = {}}, .ref = {.batch = 0, .node = 3}};
    const frontier::Entry c{.morton = 5, .header = {.min_child_error = 1.0, .max_parent_error = 4.0, .bounds = {}}, .ref = {.batch = 0, .node = 2}};

    std::vector<frontier::Entry> entries = {a, b, c};
    frontier::sort_entries(entries);

    CHECK(entries == std::vector<frontier::Entry>{c, b, a});
}

TEST_CASE("sort_batches_independently leaves every entry in its own batch", "[frontier][sort]") {
    const TempDirectory directory("sort_frontier");
    frontier::Storage run = REQUIRE_VALUE(frontier::Storage::create(directory.path()));
    write_run(run, {make_code_in_cell(4), make_code_in_cell(1), make_code_in_cell(7), make_code_in_cell(2)}, {2, 2});

    REQUIRE(frontier::sort_batches_independently(run).has_value());

    CHECK(read_codes(run) == std::vector<morton::Code>{make_code_in_cell(1), make_code_in_cell(4), make_code_in_cell(2), make_code_in_cell(7)});
}

TEST_CASE("build_histogram counts the entries of every cell", "[frontier][sort]") {
    const TempDirectory directory("sort_frontier");
    frontier::Storage input = REQUIRE_VALUE(frontier::Storage::create(directory.path()));
    write_run(input, make_codes_per_cell({2, 0, 0, 1, 0, 3, 0, 0}), {3, 3});

    const auto histogram = frontier::build_histogram(input, prefix_bits);
    REQUIRE(histogram.has_value());

    CHECK(histogram.value().prefix_bits() == prefix_bits);
    CHECK(std::ranges::equal(histogram.value().cells(), std::vector<uint64_t>{2, 0, 0, 1, 0, 3, 0, 0}));
}

TEST_CASE("assign_buckets fills buckets up to the limit", "[frontier][sort]") {
    const frontier::Buckets buckets = frontier::assign_buckets(make_histogram({1, 1, 1, 1, 1, 1, 1, 1}), 3);

    // Three cells fill a bucket, so the seventh cell opens the third one.
    CHECK(std::ranges::equal(buckets.assignment.cells(), std::vector<uint32_t>{0, 0, 0, 1, 1, 1, 2, 2}));
    CHECK(buckets.count == 3);
    CHECK(buckets.assignment.prefix_bits() == prefix_bits);
}

TEST_CASE("assign_buckets does not spend a bucket on empty cells", "[frontier][sort]") {
    const frontier::Buckets buckets = frontier::assign_buckets(make_histogram({2, 0, 0, 0, 0, 0, 0, 2}), 2);

    // The empty run between the two populated cells must not become a bucket of its own.
    CHECK(buckets.count == 2);
    CHECK(buckets.assignment.at(make_code_in_cell(0)) == 0);
    CHECK(buckets.assignment.at(make_code_in_cell(7)) == 1);
    CHECK(std::ranges::is_sorted(buckets.assignment.cells()));
}

TEST_CASE("assign_buckets keeps an oversized cell in one bucket", "[frontier][sort]") {
    const frontier::Buckets buckets = frontier::assign_buckets(make_histogram({0, 0, 10, 0, 0, 0, 0, 0}), 2);

    // Nothing splits a cell, so an oversized one still maps to a single bucket.
    CHECK(buckets.count == 1);
    CHECK(buckets.assignment.at(make_code_in_cell(2)) == buckets.assignment.at(make_code_in_cell(2, 99)));
}

TEST_CASE("scatter writes one batch per bucket in morton order", "[frontier][sort]") {
    const TempDirectory directory("sort_frontier");
    std::filesystem::create_directories(directory.path() / "in");
    std::filesystem::create_directories(directory.path() / "out");

    frontier::Storage input = REQUIRE_VALUE(frontier::Storage::create(directory.path() / "in"));
    write_run(input, make_codes_per_cell({2, 0, 0, 2, 0, 2, 0, 0}), {3, 3});

    const auto histogram = frontier::build_histogram(input, prefix_bits);
    REQUIRE(histogram.has_value());
    const frontier::Buckets buckets = frontier::assign_buckets(histogram.value(), 2);

    frontier::Storage output = REQUIRE_VALUE(frontier::Storage::create(directory.path() / "out"));
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

TEST_CASE("scatter over more buckets than it may hold open takes several passes", "[frontier][sort]") {
    const TempDirectory directory("sort_frontier");
    std::filesystem::create_directories(directory.path() / "in");
    std::filesystem::create_directories(directory.path() / "wide");
    std::filesystem::create_directories(directory.path() / "narrow");

    frontier::Storage input = REQUIRE_VALUE(frontier::Storage::create(directory.path() / "in"));
    write_run(input, make_codes_per_cell({2, 2, 2, 2, 2, 2, 2, 2}), {8, 8});

    const auto histogram = frontier::build_histogram(input, prefix_bits);
    REQUIRE(histogram.has_value());
    const frontier::Buckets buckets = frontier::assign_buckets(histogram.value(), 2);

    frontier::Storage wide = REQUIRE_VALUE(frontier::Storage::create(directory.path() / "wide"));
    REQUIRE(frontier::scatter(input, buckets, wide, 8).has_value());
    REQUIRE_VALUE(wide.save_index());

    frontier::Storage narrow = REQUIRE_VALUE(frontier::Storage::create(directory.path() / "narrow"));
    REQUIRE(frontier::scatter(input, buckets, narrow, 1).has_value());
    REQUIRE_VALUE(narrow.save_index());

    CHECK(wide.size() == narrow.size());
    CHECK(read_codes(wide) == read_codes(narrow));
}

TEST_CASE("sort_by_morton orders the whole run", "[frontier][sort]") {
    const TempDirectory directory("sort_frontier");
    std::filesystem::create_directories(directory.path() / "in");
    std::filesystem::create_directories(directory.path() / "out");

    const std::vector<morton::Code> codes = {
        make_code_in_cell(5), make_code_in_cell(0, 7), make_code_in_cell(5, 1), make_code_in_cell(2), make_code_in_cell(0), make_code_in_cell(7),
    };

    frontier::Storage input = REQUIRE_VALUE(frontier::Storage::create(directory.path() / "in"));
    write_run(input, codes, {4, 2});

    frontier::Storage output = REQUIRE_VALUE(frontier::Storage::create(directory.path() / "out"));
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

TEST_CASE("sort_by_morton handles an empty run", "[frontier][sort]") {
    const TempDirectory directory("sort_frontier");
    std::filesystem::create_directories(directory.path() / "in");
    std::filesystem::create_directories(directory.path() / "out");

    const frontier::Storage input = REQUIRE_VALUE(frontier::Storage::create(directory.path() / "in"));
    frontier::Storage output = REQUIRE_VALUE(frontier::Storage::create(directory.path() / "out"));
    const frontier::SortOptions options{.prefix_bits = prefix_bits};

    REQUIRE(frontier::sort_by_morton(input, output, options).has_value());
    CHECK(output.size() == 0);
}
