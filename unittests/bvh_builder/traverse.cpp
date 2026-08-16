#include "../catch2_helpers.h"
#include "../temp_directory.h"
#include "traverse.h"

#include <vector>

#include "Range.h"
#include "storage.h"

namespace {

frontier::Entry entry_with(const uint32_t morton) {
    return frontier::Entry{
        .morton = frontier::MortonCode{morton},
        .header = bvh::NodeHeader{
            .min_child_error = 1.0,
            .max_parent_error = 2.0,
            .triangle_count = morton,
            .bounds = bvh::Bounds{glm::dvec3{0.0}, glm::dvec3{1.0}},
        },
        .ref = bvh::Reference{.batch = 0, .node = morton},
    };
}

// `entries_per_batch[i]` entries in batch i, morton codes running 0, 1, 2, ... across the whole run.
std::vector<uint32_t> write_run(frontier::RunStorage &run, const std::vector<uint32_t> &entries_per_batch) {
    std::vector<uint32_t> written;
    for (const uint32_t count : entries_per_batch) {
        frontier::Batch batch = run.create_batch();
        for (uint32_t i = 0; i < count; i++) {
            const auto morton = static_cast<uint32_t>(written.size());
            batch.entries.push_back(entry_with(morton));
            written.push_back(morton);
        }
        REQUIRE(run.write(batch).has_value());
    }
    REQUIRE(run.save().has_value());
    return written;
}

std::vector<uint32_t> visited_mortons(
    const frontier::RunStorage &run, const AnyRange<uint32_t> batches = RangeFull{}) {
    std::vector<uint32_t> visited;
    const auto result = frontier::traverse(
        run, [&](const frontier::Entry &entry) { visited.push_back(static_cast<uint32_t>(entry.morton)); }, batches);
    REQUIRE(result.has_value());
    return visited;
}

}

TEST_CASE("traverse visits every entry in order", "[frontier]") {
    const TempDirectory directory("frontier_traverse");
    frontier::RunStorage run(directory.path());
    const std::vector<uint32_t> written = write_run(run, {3, 1, 4});

    CHECK(visited_mortons(run) == written);
}

TEST_CASE("traverse over a batch range visits only those batches", "[frontier]") {
    const TempDirectory directory("frontier_traverse");
    frontier::RunStorage run(directory.path());
    write_run(run, {3, 1, 4});

    CHECK(visited_mortons(run, Range<uint32_t>{1, 3}) == std::vector<uint32_t>{3, 4, 5, 6, 7});
    CHECK(visited_mortons(run, Range<uint32_t>{0, 1}) == std::vector<uint32_t>{0, 1, 2});
    CHECK(visited_mortons(run, Range<uint32_t>{1, 1}).empty());
}

TEST_CASE("traverse handles an empty run and empty batches", "[frontier]") {
    const TempDirectory directory("frontier_traverse");
    frontier::RunStorage run(directory.path());

    CHECK(visited_mortons(run).empty());

    write_run(run, {0, 2, 0});
    CHECK(visited_mortons(run) == std::vector<uint32_t>{0, 1});
}

TEST_CASE("traverse reports a missing batch", "[frontier]") {
    const TempDirectory directory("frontier_traverse");
    frontier::RunStorage run(directory.path());
    write_run(run, {2});

    // Reserved but never written, so reading it must fail rather than end the traversal quietly.
    run.create_batch();

    const auto result = frontier::traverse(run, [](const frontier::Entry &) {});
    CHECK_FALSE(result.has_value());
}
