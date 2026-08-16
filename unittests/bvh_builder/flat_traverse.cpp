#include "../catch2_helpers.h"
#include "../temp_directory.h"
#include "flat_traverse.h"

#include <vector>

#include "bvh_utils.h"
#include "storage.h"

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
std::vector<uint32_t> write_nodes(bvh::Storage &nodes, const std::vector<uint32_t> &nodes_per_batch) {
    std::vector<uint32_t> written;
    for (const uint32_t count : nodes_per_batch) {
        bvh::Batch batch = nodes.create_batch();
        for (uint32_t i = 0; i < count; i++) {
            const auto triangle_count = static_cast<uint32_t>(written.size());
            emit_leaf_node(batch, header_with(triangle_count), dag::Id{octree::Id::root(), triangle_count});
            written.push_back(triangle_count);
        }
        REQUIRE(nodes.write(batch).has_value());
    }
    REQUIRE(nodes.save().has_value());
    return written;
}

}

TEST_CASE("flat_traverse visits every node in order", "[bvh]") {
    const TempDirectory directory("flat_traverse");
    bvh::Storage nodes(directory.path());
    const std::vector<uint32_t> written = write_nodes(nodes, {3, 1, 4});

    std::vector<uint32_t> visited;
    const auto result = bvh::flat_traverse(nodes, [&](const bvh::Reference &, const bvh::Node &node) {
        visited.push_back(static_cast<uint32_t>(node.header.triangle_count));
    });

    REQUIRE(result.value_or(false));
    CHECK(visited == written);
}

TEST_CASE("flat_traverse names every node by the position it was read from", "[bvh]") {
    const TempDirectory directory("flat_traverse");
    bvh::Storage nodes(directory.path());
    write_nodes(nodes, {2, 1});

    std::vector<bvh::Reference> visited;
    const auto result = bvh::flat_traverse(nodes, [&](const bvh::Reference &ref, const bvh::Node &) {
        visited.push_back(ref);
    });
    REQUIRE(result.value_or(false));

    REQUIRE(visited.size() == 3);
    CHECK(visited[0].batch == 0);
    CHECK(visited[0].node == 0);
    CHECK(visited[1].batch == 0);
    CHECK(visited[1].node == 1);
    CHECK(visited[2].batch == 1);
    CHECK(visited[2].node == 0);
}

TEST_CASE("flat_traverse handles an empty store and empty batches", "[bvh]") {
    const TempDirectory directory("flat_traverse");
    bvh::Storage nodes(directory.path());

    uint32_t visits = 0;
    const auto count_visits = [&](const bvh::Reference &, const bvh::Node &) { visits++; };

    REQUIRE(bvh::flat_traverse(nodes, count_visits).value_or(false));
    CHECK(visits == 0);

    write_nodes(nodes, {0, 2, 0});
    REQUIRE(bvh::flat_traverse(nodes, count_visits).value_or(false));
    CHECK(visits == 2);
}

TEST_CASE("flat_traverse reports a missing batch", "[bvh]") {
    const TempDirectory directory("flat_traverse");
    bvh::Storage nodes(directory.path());
    write_nodes(nodes, {2});

    // Reserved but never written, so reading it must fail rather than end the traversal quietly.
    nodes.create_batch();

    const auto result = bvh::flat_traverse(nodes, [](const bvh::Reference &, const bvh::Node &) {});
    CHECK_FALSE(result.has_value());
}

TEST_CASE("a visitor returning false stops the flat walk", "[bvh]") {
    const TempDirectory directory("flat_traverse");
    bvh::Storage nodes(directory.path());
    write_nodes(nodes, {3, 2});

    uint32_t visits = 0;
    const auto stop_after_second = [&](const bvh::Reference &, const bvh::Node &) {
        visits++;
        return visits < 2;
    };
    const auto walked = bvh::flat_traverse(nodes, stop_after_second);
    REQUIRE(walked.has_value());

    CHECK(!walked.value());
    CHECK(visits == 2);
}