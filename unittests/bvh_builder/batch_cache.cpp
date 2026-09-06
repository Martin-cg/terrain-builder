#include "../catch2_helpers.h"
#include "../temp_directory.h"

#include <cstdint>

#include "bvh/Storage.h"
#include "bvh/layout.h"
#include "storage/BatchCache.h"

namespace {

bvh::NodeHeader header_with(const uint32_t triangle_count) {
    return bvh::NodeHeader{
        .min_child_error = 1.0,
        .max_parent_error = 2.0,
        .triangle_count = triangle_count,
        .bounds = bvh::Bounds{glm::dvec3{0.0}, glm::dvec3{1.0}},
    };
}

// One leaf per batch, its triangle count naming the batch it landed in.
void write_batches(bvh::Storage &nodes, const uint32_t count) {
    for (uint32_t index = 0; index < count; index++) {
        bvh::Batch batch = nodes.create_batch();
        batch.add_leaf_node(header_with(index), dag::Id{octree::Id::root(), index});
        REQUIRE(nodes.write(batch).has_value());
    }
    REQUIRE(nodes.save_index().has_value());
}

}

TEST_CASE("a batch read twice only goes to disk once", "[bvh]") {
    const TempDirectory directory("batch_cache");
    bvh::Storage nodes(directory.path());
    write_batches(nodes, 3);

    BatchCache<bvh::Storage> cache(nodes, 2);

    REQUIRE(cache.read_batch(0).has_value());
    REQUIRE(cache.read_batch(0).has_value());

    CHECK(cache.count_misses() == 1);
    CHECK(cache.count_resident() == 1);
}

TEST_CASE("a resident batch is handed out without being copied", "[bvh]") {
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

TEST_CASE("a batch read back is the one that was written", "[bvh]") {
    const TempDirectory directory("batch_cache");
    bvh::Storage nodes(directory.path());
    write_batches(nodes, 3);

    BatchCache<bvh::Storage> cache(nodes, 2);

    const auto result = cache.read_batch(2);
    REQUIRE(result.has_value());

    const bvh::Batch &batch = result->get();
    REQUIRE(batch.nodes().size() == 1);
    CHECK(batch.index() == 2);
    CHECK(batch.nodes()[0].header.triangle_count == 2);
}

TEST_CASE("the resident set never grows past the budget", "[bvh]") {
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

TEST_CASE("the least recently used batch is the one evicted", "[bvh]") {
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