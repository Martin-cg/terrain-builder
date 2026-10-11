#include "../catch2_helpers.h"
#include "../result_helpers.h"
#include "../temp_directory.h"

#include <array>
#include <cstdint>
#include <span>
#include <vector>

#include <glm/glm.hpp>

#include "bvh/BatchWriter.h"
#include "bvh/NodeCache.h"
#include "bvh/Storage.h"
#include "bvh/layout.h"
#include "widen/options.h"
#include "widen/tabulate_splits.h"
#include "widen/write_wide_nodes.h"

namespace {

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

TEST_CASE("a kept root collects every leaf its dissolved children held", "[widen][write_wide_nodes]") {
    const TempDirectory directory("write_wide_nodes");
    bvh::Storage nodes = REQUIRE_VALUE(bvh::Storage::create(directory.path() / "input"));
    bvh::BatchWriter writer(nodes);
    const bvh::Reference leaf_0 = REQUIRE_VALUE(writer.write_leaf({}, dag::Id{octree::Id::root(), 0}));
    const bvh::Reference leaf_1 = REQUIRE_VALUE(writer.write_leaf({}, dag::Id{octree::Id::root(), 1}));
    const bvh::Reference leaf_2 = REQUIRE_VALUE(writer.write_leaf({}, dag::Id{octree::Id::root(), 2}));
    const bvh::Reference leaf_3 = REQUIRE_VALUE(writer.write_leaf({}, dag::Id{octree::Id::root(), 3}));
    const bvh::Reference a = REQUIRE_VALUE(writer.write_inner({}, std::array{leaf_0, leaf_1}));
    const bvh::Reference b = REQUIRE_VALUE(writer.write_inner({}, std::array{leaf_2, leaf_3}));
    const bvh::Reference root = REQUIRE_VALUE(writer.write_inner({}, std::array{a, b}));
    REQUIRE(writer.flush().has_value());
    REQUIRE(nodes.save_index().has_value());

    // The root keeps itself and offers A and B two slots each, at which both dissolve.
    widen::SplitStorage splits = REQUIRE_VALUE(widen::SplitStorage::create(directory.path() / "splits"));
    const std::array root_row{widen::Split::keep(2), widen::Split::dissolve(1), widen::Split::dissolve(1)};
    const std::array child_row{widen::Split::keep(1)};
    write_splits(splits, root_row, child_row, child_row);

    bvh::Storage output = REQUIRE_VALUE(bvh::Storage::create(directory.path() / "output"));
    bvh::NodeCache cache(nodes, 1);
    const auto wide_root = widen::write_wide_nodes(splits, cache, root, output, widen::Options{.arity = 4});
    REQUIRE(wide_root.has_value());
    REQUIRE_VALUE(output.save_index());

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

TEST_CASE("a kept node keeps the header it had", "[widen][write_wide_nodes]") {
    const bvh::NodeHeader root_header{.min_child_error = 1.0, .max_parent_error = 2.0, .bounds = {glm::dvec3{0.0}, glm::dvec3{4.0}}};

    const TempDirectory directory("write_wide_nodes");
    bvh::Storage nodes = REQUIRE_VALUE(bvh::Storage::create(directory.path() / "input"));
    bvh::BatchWriter writer(nodes);
    const bvh::Reference leaf_0 = REQUIRE_VALUE(writer.write_leaf({}, dag::Id{octree::Id::root(), 0}));
    const bvh::Reference leaf_1 = REQUIRE_VALUE(writer.write_leaf({}, dag::Id{octree::Id::root(), 1}));
    const bvh::Reference leaf_2 = REQUIRE_VALUE(writer.write_leaf({}, dag::Id{octree::Id::root(), 2}));
    const bvh::Reference leaf_3 = REQUIRE_VALUE(writer.write_leaf({}, dag::Id{octree::Id::root(), 3}));
    const bvh::Reference a = REQUIRE_VALUE(writer.write_inner({}, std::array{leaf_0, leaf_1}));
    const bvh::Reference b = REQUIRE_VALUE(writer.write_inner({}, std::array{leaf_2, leaf_3}));
    const bvh::Reference root = REQUIRE_VALUE(writer.write_inner(root_header, std::array{a, b}));
    REQUIRE(writer.flush().has_value());
    REQUIRE(nodes.save_index().has_value());

    widen::SplitStorage splits = REQUIRE_VALUE(widen::SplitStorage::create(directory.path() / "splits"));
    const std::array root_row{widen::Split::keep(2), widen::Split::dissolve(1), widen::Split::dissolve(1)};
    const std::array child_row{widen::Split::keep(1)};
    write_splits(splits, root_row, child_row, child_row);

    bvh::Storage output = REQUIRE_VALUE(bvh::Storage::create(directory.path() / "output"));
    bvh::NodeCache cache(nodes, 1);
    const auto wide_root = widen::write_wide_nodes(splits, cache, root, output, widen::Options{.arity = 4});
    REQUIRE(wide_root.has_value());
    REQUIRE_VALUE(output.save_index());

    const auto root_batch = output.read(wide_root->batch);
    REQUIRE(root_batch.has_value());
    CHECK(root_batch->node(wide_root->node).header == root_header);
}

TEST_CASE("children that keep their slot stay nodes of their own", "[widen][write_wide_nodes]") {
    const TempDirectory directory("write_wide_nodes");
    bvh::Storage nodes = REQUIRE_VALUE(bvh::Storage::create(directory.path() / "input"));
    bvh::BatchWriter writer(nodes);
    const bvh::Reference leaf_0 = REQUIRE_VALUE(writer.write_leaf({}, dag::Id{octree::Id::root(), 0}));
    const bvh::Reference leaf_1 = REQUIRE_VALUE(writer.write_leaf({}, dag::Id{octree::Id::root(), 1}));
    const bvh::Reference leaf_2 = REQUIRE_VALUE(writer.write_leaf({}, dag::Id{octree::Id::root(), 2}));
    const bvh::Reference leaf_3 = REQUIRE_VALUE(writer.write_leaf({}, dag::Id{octree::Id::root(), 3}));
    const bvh::Reference a = REQUIRE_VALUE(writer.write_inner({}, std::array{leaf_0, leaf_1}));
    const bvh::Reference b = REQUIRE_VALUE(writer.write_inner({}, std::array{leaf_2, leaf_3}));
    const bvh::Reference root = REQUIRE_VALUE(writer.write_inner({}, std::array{a, b}));
    REQUIRE(writer.flush().has_value());
    REQUIRE(nodes.save_index().has_value());

    // At arity two nothing can be widened, so every inner node keeps its slot and its row is one entry long.
    widen::SplitStorage splits = REQUIRE_VALUE(widen::SplitStorage::create(directory.path() / "splits"));
    const std::array row{widen::Split::keep(1)};
    write_splits(splits, row, row, row);

    bvh::Storage output = REQUIRE_VALUE(bvh::Storage::create(directory.path() / "output"));
    bvh::NodeCache cache(nodes, 1);
    const auto wide_root = widen::write_wide_nodes(splits, cache, root, output, widen::Options{.arity = 2});
    REQUIRE(wide_root.has_value());
    REQUIRE_VALUE(output.save_index());

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