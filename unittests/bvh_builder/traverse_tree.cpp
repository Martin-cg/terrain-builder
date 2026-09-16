#include "../catch2_helpers.h"
#include "../temp_directory.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include <glm/glm.hpp>

#include "enumerate.h"
#include "range_utils.h"

#include "bvh/BatchWriter.h"
#include "bvh/NodeCache.h"
#include "bvh/Storage.h"
#include "bvh/layout.h"
#include "bvh/traverse_tree.h"
#include "storage/batch_limits.h"

namespace {

const bvh::NodeHeader root_header{.min_child_error = 1.0, .max_parent_error = 2.0, .bounds = {glm::dvec3{0.0}, glm::dvec3{4.0}}};

// The tree R(A(0, 1), B(2, 3)) as it was written.
struct BalancedTree {
    bvh::Reference root;
    bvh::Reference left;
    bvh::Reference right;
    std::array<bvh::Reference, 4> leaves;
};

// Writes R(A(0, 1), B(2, 3)) through `writer`, with leaf i naming cluster i and only the root carrying a header.
BalancedTree write_balanced_tree(bvh::BatchWriter &writer) {
    BalancedTree tree;
    for (auto &[cluster, leaf] : enumerate<uint32_t>(tree.leaves)) {
        const auto leaf_result = writer.write_leaf(bvh::NodeHeader{}, dag::Id{octree::Id::root(), cluster});
        REQUIRE(leaf_result.has_value());
        leaf = *leaf_result;
    }

    const auto left_result = writer.write_inner(bvh::NodeHeader{}, std::array{tree.leaves[0], tree.leaves[1]});
    REQUIRE(left_result.has_value());
    tree.left = *left_result;

    const auto right_result = writer.write_inner(bvh::NodeHeader{}, std::array{tree.leaves[2], tree.leaves[3]});
    REQUIRE(right_result.has_value());
    tree.right = *right_result;

    const auto root_result = writer.write_inner(root_header, std::array{tree.left, tree.right});
    REQUIRE(root_result.has_value());
    tree.root = *root_result;

    REQUIRE(writer.flush().has_value());
    return tree;
}

}

TEST_CASE("the walk visits every node once, each parent before its children and children in stored order", "[bvh][traverse_tree]") {
    // Spread over one batch per node as well, with room for a single one in memory, so the order cannot come from the batches.
    const size_t max_batch_bytes = GENERATE(default_max_batch_bytes, size_t{1});

    const TempDirectory directory("traverse_tree");
    bvh::Storage nodes(directory.path());
    bvh::BatchWriter writer(nodes, max_batch_bytes);
    const BalancedTree tree = write_balanced_tree(writer);
    REQUIRE(nodes.save_index().has_value());

    bvh::NodeCache cache(nodes, 1);
    std::vector<bvh::Reference> visited;
    const auto result = bvh::traverse_tree(cache, tree.root, [&](const bvh::Reference &reference, const bvh::NodeView &) {
        visited.push_back(reference);
    });

    REQUIRE(result.value_or(false));
    CHECK(visited == std::vector{tree.root, tree.left, tree.leaves[0], tree.leaves[1], tree.right, tree.leaves[2], tree.leaves[3]});
}

TEST_CASE("a leaf root is visited alone, with its header and cluster", "[bvh][traverse_tree]") {
    const bvh::NodeHeader leaf_header{.min_child_error = 3.0, .max_parent_error = 4.0, .bounds = {glm::dvec3{5.0}, glm::dvec3{6.0}}};

    const TempDirectory directory("traverse_tree");
    bvh::Storage nodes(directory.path());
    bvh::BatchWriter writer(nodes);
    const auto leaf_result = writer.write_leaf(leaf_header, dag::Id{octree::Id::root(), 5});
    REQUIRE(leaf_result.has_value());
    REQUIRE(writer.flush().has_value());
    REQUIRE(nodes.save_index().has_value());

    bvh::NodeCache cache(nodes, 1);
    std::vector<bvh::Reference> visited;
    bvh::NodeHeader header;
    std::optional<dag::Id> cluster;
    const auto result = bvh::traverse_tree(cache, *leaf_result, [&](const bvh::Reference &reference, const bvh::NodeView &view) {
        visited.push_back(reference);
        header = view.header();
        cluster = view.leaf();
    });

    REQUIRE(result.value_or(false));
    CHECK(visited == std::vector{*leaf_result});
    CHECK(header == leaf_header);
    CHECK(cluster == dag::Id{octree::Id::root(), 5});
}

TEST_CASE("an inner node's view carries its header and children", "[bvh][traverse_tree]") {
    const TempDirectory directory("traverse_tree");
    bvh::Storage nodes(directory.path());
    bvh::BatchWriter writer(nodes);
    const BalancedTree tree = write_balanced_tree(writer);
    REQUIRE(nodes.save_index().has_value());

    bvh::NodeCache cache(nodes, 1);
    bvh::NodeHeader header;
    std::vector<bvh::Reference> children;
    const auto result = bvh::traverse_tree(cache, tree.root, [&](const bvh::Reference &, const bvh::NodeView &view) {
        header = view.header();
        children.assign_range(view.children());
        return false;
    });

    REQUIRE(result.has_value());
    CHECK(header == root_header);
    CHECK(children == std::vector{tree.left, tree.right});
}

TEST_CASE("a view stays valid while the visitor reads through the same cache", "[bvh][traverse_tree]") {
    const TempDirectory directory("traverse_tree");
    bvh::Storage nodes(directory.path());
    // One node per batch and room for one batch in memory, so every read below evicts the root's batch.
    bvh::BatchWriter writer(nodes, 1);
    const BalancedTree tree = write_balanced_tree(writer);
    REQUIRE(nodes.save_index().has_value());

    bvh::NodeCache cache(nodes, 1);
    bvh::NodeHeader header;
    std::vector<bvh::Reference> children;
    const auto result = bvh::traverse_tree(cache, tree.root, [&](const bvh::Reference &, const bvh::NodeView &view) {
        for (const uint32_t batch : range(nodes.size())) {
            REQUIRE(cache.read_batch(batch).has_value());
        }
        header = view.header();
        children.assign_range(view.children());
        return false;
    });

    REQUIRE(result.has_value());
    CHECK(header == root_header);
    CHECK(children == std::vector{tree.left, tree.right});
}

TEST_CASE("a visitor returning false stops the walk", "[bvh][traverse_tree]") {
    const TempDirectory directory("traverse_tree");
    bvh::Storage nodes(directory.path());
    bvh::BatchWriter writer(nodes);
    const BalancedTree tree = write_balanced_tree(writer);
    REQUIRE(nodes.save_index().has_value());

    bvh::NodeCache cache(nodes, 1);
    uint32_t visits = 0;
    const auto result = bvh::traverse_tree(cache, tree.root, [&](const bvh::Reference &, const bvh::NodeView &) {
        visits++;
        return visits < 3;
    });

    REQUIRE(result.has_value());
    CHECK_FALSE(result.value());
    CHECK(visits == 3);
}

TEST_CASE("a child whose batch cannot be read fails the walk", "[bvh][traverse_tree]") {
    const TempDirectory directory("traverse_tree");
    bvh::Storage nodes(directory.path());

    // Reserved but never written, so reading it must fail rather than end the walk quietly.
    const bvh::Batch missing = nodes.create_batch();

    bvh::BatchWriter writer(nodes);
    const auto root_result = writer.write_inner(bvh::NodeHeader{},std::array{bvh::Reference{.batch = missing.index(), .node = 0}});
    REQUIRE(root_result.has_value());
    REQUIRE(writer.flush().has_value());
    REQUIRE(nodes.save_index().has_value());

    bvh::NodeCache cache(nodes, 1);
    uint32_t visits = 0;
    const auto result = bvh::traverse_tree(cache, *root_result, [&](const bvh::Reference &, const bvh::NodeView &) {
        visits++;
    });

    CHECK_FALSE(result.has_value());
    CHECK(visits == 1);
}