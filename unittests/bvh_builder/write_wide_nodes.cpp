#include "../catch2_helpers.h"
#include "../temp_directory.h"

#include <array>
#include <cstdint>
#include <span>
#include <vector>

#include <glm/glm.hpp>

#include "enumerate.h"

#include "bvh/BatchWriter.h"
#include "bvh/NodeCache.h"
#include "bvh/Storage.h"
#include "bvh/layout.h"
#include "widen/options.h"
#include "widen/tabulate_splits.h"
#include "widen/write_wide_nodes.h"

namespace {

const bvh::NodeHeader root_header{.min_child_error = 1.0, .max_parent_error = 2.0, .bounds = {glm::dvec3{0.0}, glm::dvec3{4.0}}};

// Writes the tree R(A(0, 1), B(2, 3)) into `nodes`, with leaf i naming cluster i, and returns its root.
bvh::Reference write_balanced_tree(bvh::Storage &nodes) {
    bvh::BatchWriter writer(nodes);
    std::array<bvh::Reference, 4> leaves;
    for (auto &[cluster, leaf] : enumerate<uint32_t>(leaves)) {
        const auto leaf_result = writer.write_leaf(bvh::NodeHeader{}, dag::Id{octree::Id::root(), cluster});
        REQUIRE(leaf_result.has_value());
        leaf = *leaf_result;
    }
    const auto a_result = writer.write_inner(bvh::NodeHeader{}, std::array{leaves[0], leaves[1]});
    REQUIRE(a_result.has_value());
    const auto b_result = writer.write_inner(bvh::NodeHeader{}, std::array{leaves[2], leaves[3]});
    REQUIRE(b_result.has_value());
    const auto root_result = writer.write_inner(root_header, std::array{*a_result, *b_result});
    REQUIRE(root_result.has_value());

    REQUIRE(writer.flush().has_value());
    REQUIRE(nodes.save_index().has_value());
    return *root_result;
}

// Stores the split rows of R, A and B in the order the tabulation leaves them: B, A, then R.
void write_splits(
    widen::SplitStorage &splits, const std::span<const widen::Split> root_row,
    const std::span<const widen::Split> a_row, const std::span<const widen::Split> b_row) {
    widen::SplitBatch batch;
    batch.splits.push_row(b_row);
    batch.splits.push_row(a_row);
    batch.splits.push_row(root_row);
    REQUIRE(splits.write(batch).has_value());
    REQUIRE(splits.save_index().has_value());
}

}

TEST_CASE("a kept root collects every leaf its dissolved children held", "[write_wide_nodes]") {
    const TempDirectory directory("write_wide_nodes");
    bvh::Storage nodes(directory.path() / "input");
    const bvh::Reference root = write_balanced_tree(nodes);

    // The root keeps itself and offers A and B two slots each, at which both dissolve.
    widen::SplitStorage splits(directory.path() / "splits");
    const std::array root_row{widen::Split::keep(2), widen::Split::dissolve(1), widen::Split::dissolve(1)};
    const std::array child_row{widen::Split::keep(1)};
    write_splits(splits, root_row, child_row, child_row);

    bvh::Storage output(directory.path() / "output");
    bvh::NodeCache cache(nodes, 1);
    const auto wide_root = widen::write_wide_nodes(splits, cache, root, output, widen::Options{.arity = 4});
    REQUIRE(wide_root.has_value());

    const auto root_batch = output.read(wide_root->batch);
    REQUIRE(root_batch.has_value());
    std::vector<dag::Id> clusters;
    for (const bvh::Reference &child : root_batch->resolve(root_batch->node(wide_root->node)).children()) {
        const auto child_batch = output.read(child.batch);
        REQUIRE(child_batch.has_value());
        clusters.push_back(child_batch->resolve(child_batch->node(child.node)).leaf());
    }
    CHECK(clusters == std::vector{
        dag::Id{octree::Id::root(), 0},
        dag::Id{octree::Id::root(), 1},
        dag::Id{octree::Id::root(), 2},
        dag::Id{octree::Id::root(), 3},
    });
}

TEST_CASE("a kept node keeps the header it had", "[write_wide_nodes]") {
    const TempDirectory directory("write_wide_nodes");
    bvh::Storage nodes(directory.path() / "input");
    const bvh::Reference root = write_balanced_tree(nodes);

    widen::SplitStorage splits(directory.path() / "splits");
    const std::array root_row{widen::Split::keep(2), widen::Split::dissolve(1), widen::Split::dissolve(1)};
    const std::array child_row{widen::Split::keep(1)};
    write_splits(splits, root_row, child_row, child_row);

    bvh::Storage output(directory.path() / "output");
    bvh::NodeCache cache(nodes, 1);
    const auto wide_root = widen::write_wide_nodes(splits, cache, root, output, widen::Options{.arity = 4});
    REQUIRE(wide_root.has_value());

    const auto root_batch = output.read(wide_root->batch);
    REQUIRE(root_batch.has_value());
    CHECK(root_batch->node(wide_root->node).header == root_header);
}

TEST_CASE("children that keep their slot stay nodes of their own", "[write_wide_nodes]") {
    const TempDirectory directory("write_wide_nodes");
    bvh::Storage nodes(directory.path() / "input");
    const bvh::Reference root = write_balanced_tree(nodes);

    // At arity two nothing can be widened, so every inner node keeps its slot and its row is one entry long.
    widen::SplitStorage splits(directory.path() / "splits");
    const std::array row{widen::Split::keep(1)};
    write_splits(splits, row, row, row);

    bvh::Storage output(directory.path() / "output");
    bvh::NodeCache cache(nodes, 1);
    const auto wide_root = widen::write_wide_nodes(splits, cache, root, output, widen::Options{.arity = 2});
    REQUIRE(wide_root.has_value());

    const auto root_batch = output.read(wide_root->batch);
    REQUIRE(root_batch.has_value());
    const std::span<const bvh::Reference> children = root_batch->resolve(root_batch->node(wide_root->node)).children();
    REQUIRE(children.size() == 2);
    for (const bvh::Reference &child : children) {
        const auto child_batch = output.read(child.batch);
        REQUIRE(child_batch.has_value());
        CHECK(child_batch->resolve(child_batch->node(child.node)).children().size() == 2);
    }
}