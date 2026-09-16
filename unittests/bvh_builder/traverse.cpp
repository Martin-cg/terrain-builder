#include "../catch2_helpers.h"
#include "../temp_directory.h"

#include <vector>

#include "Range.h"
#include "frontier/Storage.h"
#include "frontier/traverse.h"

namespace {

frontier::Entry make_entry(const uint32_t morton) {
    return frontier::Entry{
        .morton = morton::Code{morton},
        .header = {},
        .ref = {},
    };
}

// `entries_per_batch[i]` entries in batch i, morton codes running 0, 1, 2, ... across the whole run.
std::vector<uint32_t> write_run(frontier::Storage &run, const std::vector<uint32_t> &entries_per_batch) {
    std::vector<uint32_t> written;
    for (const uint32_t count : entries_per_batch) {
        frontier::Batch batch;
        for (uint32_t i = 0; i < count; i++) {
            const auto morton = static_cast<uint32_t>(written.size());
            batch.entries.push_back(make_entry(morton));
            written.push_back(morton);
        }
        REQUIRE(run.write(batch).has_value());
    }
    REQUIRE(run.save_index().has_value());
    return written;
}

std::vector<uint32_t> collect_mortons(
    const frontier::Storage &run, const AnyRange<uint32_t> batches = RangeFull{}) {
    std::vector<uint32_t> visited;
    const auto result = frontier::traverse_entries(
        run, [&](const frontier::Entry &entry) { visited.push_back(static_cast<uint32_t>(entry.morton)); }, batches);
    REQUIRE(result.has_value());
    return visited;
}

}

TEST_CASE("traverse visits every entry in order", "[frontier_traverse]") {
    const TempDirectory directory("frontier_traverse");
    frontier::Storage run(directory.path());
    const std::vector<uint32_t> written = write_run(run, {3, 1, 4});

    CHECK(collect_mortons(run) == written);
}

TEST_CASE("traverse over a batch range visits only those batches", "[frontier_traverse]") {
    const TempDirectory directory("frontier_traverse");
    frontier::Storage run(directory.path());
    write_run(run, {3, 1, 4});

    CHECK(collect_mortons(run, Range<uint32_t>{1, 3}) == std::vector<uint32_t>{3, 4, 5, 6, 7});
    CHECK(collect_mortons(run, Range<uint32_t>{0, 1}) == std::vector<uint32_t>{0, 1, 2});
    CHECK(collect_mortons(run, Range<uint32_t>{1, 1}).empty());
}

TEST_CASE("traverse handles an empty run and empty batches", "[frontier_traverse]") {
    const TempDirectory directory("frontier_traverse");
    frontier::Storage run(directory.path());

    CHECK(collect_mortons(run).empty());

    write_run(run, {0, 2, 0});
    CHECK(collect_mortons(run) == std::vector<uint32_t>{0, 1});
}
