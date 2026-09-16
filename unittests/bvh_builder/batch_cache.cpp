#include "../catch2_helpers.h"
#include "../temp_directory.h"

#include <cstdint>
#include <utility>
#include <vector>

#include "bvh/Storage.h"
#include "bvh/layout.h"
#include "storage/BatchCache.h"

namespace {

// One leaf per batch, naming the batch it landed in, and returns the batches as written.
std::vector<bvh::Batch> write_batches(bvh::Storage &nodes, const uint32_t count) {
    std::vector<bvh::Batch> written;
    for (uint32_t index = 0; index < count; index++) {
        bvh::Batch batch = nodes.create_batch();
        batch.add_leaf_node(bvh::NodeHeader{}, dag::Id{octree::Id::root(), index});
        REQUIRE(nodes.write(batch).has_value());
        written.push_back(std::move(batch));
    }
    REQUIRE(nodes.save_index().has_value());
    return written;
}

}

TEST_CASE("a batch read twice only goes to disk once", "[batch_cache]") {
    const TempDirectory directory("batch_cache");
    bvh::Storage nodes(directory.path());
    write_batches(nodes, 3);

    BatchCache<bvh::Storage> cache(nodes, 2);

    REQUIRE(cache.read_batch(0).has_value());
    REQUIRE(cache.read_batch(0).has_value());

    CHECK(cache.count_misses() == 1);
    CHECK(cache.count_resident() == 1);
}

TEST_CASE("a resident batch is handed out without being copied", "[batch_cache]") {
    const TempDirectory directory("batch_cache");
    bvh::Storage nodes(directory.path());
    write_batches(nodes, 3);

    BatchCache<bvh::Storage> cache(nodes, 2);

    const auto first_result = cache.read_batch(1);
    REQUIRE(first_result.has_value());
    const auto second_result = cache.read_batch(1);
    REQUIRE(second_result.has_value());

    CHECK(&first_result->get() == &second_result->get());
}

TEST_CASE("a batch read back is the one that was written", "[batch_cache]") {
    const TempDirectory directory("batch_cache");
    bvh::Storage nodes(directory.path());
    const std::vector<bvh::Batch> written = write_batches(nodes, 3);

    BatchCache<bvh::Storage> cache(nodes, 2);

    const auto result = cache.read_batch(2);
    REQUIRE(result.has_value());
    CHECK(result->get() == written[2]);
}

TEST_CASE("the resident set never grows past the budget", "[batch_cache]") {
    const TempDirectory directory("batch_cache");
    bvh::Storage nodes(directory.path());
    write_batches(nodes, 5);

    BatchCache<bvh::Storage> cache(nodes, 2);

    for (uint32_t index = 0; index < 5; index++) {
        REQUIRE(cache.read_batch(index).has_value());
        REQUIRE(cache.count_resident() <= 2);
    }

    CHECK(cache.count_misses() == 5);
}

TEST_CASE("the least recently used batch is the one evicted", "[batch_cache]") {
    const TempDirectory directory("batch_cache");
    bvh::Storage nodes(directory.path());
    write_batches(nodes, 3);

    BatchCache<bvh::Storage> cache(nodes, 2);

    REQUIRE(cache.read_batch(0).has_value());
    REQUIRE(cache.read_batch(1).has_value());
    // Reading 0 again makes 1 the older of the two, so admitting 2 must drop 1.
    REQUIRE(cache.read_batch(0).has_value());
    REQUIRE(cache.read_batch(2).has_value());
    REQUIRE(cache.count_misses() == 3);

    REQUIRE(cache.read_batch(0).has_value());
    CHECK(cache.count_misses() == 3);

    REQUIRE(cache.read_batch(1).has_value());
    CHECK(cache.count_misses() == 4);
}