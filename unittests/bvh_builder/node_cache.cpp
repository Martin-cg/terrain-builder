#include "../catch2_helpers.h"
#include "../temp_directory.h"

#include <cstdint>
#include <vector>

#include "bvh/NodeCache.h"
#include "bvh/Storage.h"
#include "bvh/layout.h"

namespace {

bvh::NodeHeader header_with(const uint32_t triangle_count) {
    return bvh::NodeHeader{
        .min_child_error = 1.0,
        .max_parent_error = 2.0,
        .triangle_count = triangle_count,
        .bounds = bvh::Bounds{glm::dvec3{0.0}, glm::dvec3{1.0}},
    };
}

// `nodes_per_batch[i]` leaves in batch i, triangle counts running 0, 1, 2, ... across the whole store.
void write_leaves(bvh::Storage &nodes, const std::vector<uint32_t> &nodes_per_batch) {
    uint32_t written = 0;
    for (const uint32_t count : nodes_per_batch) {
        bvh::Batch batch = nodes.create_batch();
        for (uint32_t i = 0; i < count; i++) {
            batch.add_leaf_node(header_with(written), dag::Id{octree::Id::root(), written});
            written++;
        }
        REQUIRE(nodes.write(batch).has_value());
    }
    REQUIRE(nodes.save_index().has_value());
}

}

TEST_CASE("a node comes back from the batch it was written to", "[bvh]") {
    const TempDirectory directory("node_cache");
    bvh::Storage nodes(directory.path());
    write_leaves(nodes, {2, 3});

    bvh::NodeCache cache(nodes, 2);

    const auto node = cache.read_node(bvh::Reference{.batch = 1, .node = 2});
    REQUIRE(node.has_value());
    CHECK(node->header.triangle_count == 4);
    CHECK(node->is_leaf());
}

TEST_CASE("nodes in the same batch are read once", "[bvh]") {
    const TempDirectory directory("node_cache");
    bvh::Storage nodes(directory.path());
    write_leaves(nodes, {2, 3});

    bvh::NodeCache cache(nodes, 2);

    REQUIRE(cache.read_node(bvh::Reference{.batch = 1, .node = 0}).has_value());
    REQUIRE(cache.read_node(bvh::Reference{.batch = 1, .node = 1}).has_value());
    REQUIRE(cache.read_node(bvh::Reference{.batch = 1, .node = 2}).has_value());

    CHECK(cache.count_misses() == 1);
}

TEST_CASE("a node outlives the read that follows it", "[bvh]") {
    const TempDirectory directory("node_cache");
    bvh::Storage nodes(directory.path());
    write_leaves(nodes, {1, 1, 1});

    bvh::NodeCache cache(nodes, 1);

    const auto first = cache.read_node(bvh::Reference{.batch = 0, .node = 0});
    REQUIRE(first.has_value());
    REQUIRE(cache.read_node(bvh::Reference{.batch = 2, .node = 0}).has_value());

    CHECK(first->header.triangle_count == 0);
}

TEST_CASE("the batch behind a node is reachable through the cache", "[bvh]") {
    const TempDirectory directory("node_cache");
    bvh::Storage nodes(directory.path());
    write_leaves(nodes, {2, 3});

    bvh::NodeCache cache(nodes, 2);

    const auto result = cache.read_batch(1);
    REQUIRE(result.has_value());

    const bvh::Batch &batch = result->get();
    REQUIRE(batch.leaves().size() == 3);
    CHECK(batch.leaves()[0].cluster_index == 2);
}