#include "../catch2_helpers.h"
#include "../temp_directory.h"

#include <array>
#include <cstdint>
#include <span>
#include <vector>

#include "enumerate.h"

#include "bvh/NodeCache.h"
#include "bvh/Storage.h"
#include "bvh/layout.h"
#include "bvh/utils.h"
#include "widen/options.h"
#include "widen/tabulate_splits.h"
#include "widen/write_wide_nodes.h"

namespace {

bvh::NodeHeader leaf_header(const uint32_t index) {
    const auto offset = static_cast<double>(index);
    return bvh::NodeHeader{
        .min_child_error = 1.0,
        .max_parent_error = 2.0,
        .triangle_count = index,
        .bounds = bvh::Bounds{glm::dvec3{offset, 0.0, 0.0}, glm::dvec3{offset + 1.0, 1.0, 1.0}},
    };
}

// The tree R(A(0, 1), B(2, 3)) the handwritten rows below are addressed to.
bvh::Reference write_balanced_tree(bvh::Storage &nodes) {
    std::array<bvh::NodeHeader, 4> leaves;
    bvh::Batch leaf_batch = nodes.create_batch();
    for (auto &[index, header] : enumerate<uint32_t>(leaves)) {
        header = leaf_header(index);
        leaf_batch.add_leaf_node(header, dag::Id{octree::Id::root(), index});
    }
    const bvh::BatchIndex leaf_index = leaf_batch.index();
    REQUIRE(nodes.write(leaf_batch).has_value());

    const bvh::NodeHeader left_header = bvh::merge_headers(std::array{leaves[0], leaves[1]});
    const bvh::NodeHeader right_header = bvh::merge_headers(std::array{leaves[2], leaves[3]});

    bvh::Batch inner_batch = nodes.create_batch();
    inner_batch.add_inner_node(left_header, std::array{
        bvh::Reference{.batch = leaf_index, .node = 0},
        bvh::Reference{.batch = leaf_index, .node = 1},
    });
    inner_batch.add_inner_node(right_header, std::array{
        bvh::Reference{.batch = leaf_index, .node = 2},
        bvh::Reference{.batch = leaf_index, .node = 3},
    });
    const bvh::BatchIndex inner_index = inner_batch.index();
    REQUIRE(nodes.write(inner_batch).has_value());

    bvh::Batch root_batch = nodes.create_batch();
    const bvh::NodeIndex root = root_batch.add_inner_node(bvh::merge_headers(std::array{left_header, right_header}), std::array{
        bvh::Reference{.batch = inner_index, .node = 0},
        bvh::Reference{.batch = inner_index, .node = 1},
    });
    const bvh::BatchIndex root_index = root_batch.index();
    REQUIRE(nodes.write(root_batch).has_value());

    REQUIRE(nodes.save_index().has_value());
    return bvh::Reference{.batch = root_index, .node = root};
}

// One row per inner node in the order the tabulation writes them, right subtree first: B, A, then R.
void write_splits(widen::SplitStorage &splits, const std::span<const widen::Split> inner_row, const std::span<const widen::Split> root_row) {
    widen::SplitBatch batch;
    batch.splits.push_row(inner_row);
    batch.splits.push_row(inner_row);
    batch.splits.push_row(root_row);

    REQUIRE(splits.write(batch).has_value());
    REQUIRE(splits.save_index().has_value());
}

// The references a node points at, resolved through `output`.
std::vector<bvh::Reference> children_of(const bvh::Storage &output, const bvh::Reference &reference) {
    const auto result = output.read(reference.batch);
    REQUIRE(result.has_value());

    const bvh::Node &node = result->nodes()[reference.node];
    const std::span<const bvh::Reference> children = result->children().subspan(node.child_start, node.child_count);
    return std::vector<bvh::Reference>(children.begin(), children.end());
}

// The leaf id a node stands for, resolved through `output`.
dag::Id leaf_id_of(const bvh::Storage &output, const bvh::Reference &reference) {
    const auto result = output.read(reference.batch);
    REQUIRE(result.has_value());

    const bvh::Node &node = result->nodes()[reference.node];
    REQUIRE(node.is_leaf());
    return result->leaves()[node.child_start];
}

}

TEST_CASE("a kept root collects every leaf its dissolved children held", "[widen]") {
    const TempDirectory directory("write_wide_nodes");
    bvh::Storage nodes(directory.path() / "input");
    const bvh::Reference root = write_balanced_tree(nodes);

    // A and B only have a row at one slot; offered two they fall past its end and dissolve.
    widen::SplitStorage splits(directory.path() / "splits");
    const std::array inner_row{widen::Split::keep(1)};
    const std::array root_row{
        widen::Split::keep(2),
        widen::Split::dissolve(1),
        widen::Split::dissolve(1),
    };
    write_splits(splits, inner_row, root_row);

    bvh::Storage output(directory.path() / "output");
    bvh::NodeCache cache(nodes, 4);
    const auto result = widen::write_wide_nodes(splits, cache, root, output, widen::Options{.arity = 4});
    REQUIRE(result.has_value());

    const std::vector<bvh::Reference> children = children_of(output, *result);
    REQUIRE(children.size() == 4);
    for (const auto &[index, child] : enumerate<uint32_t>(children)) {
        REQUIRE(leaf_id_of(output, child) == dag::Id{octree::Id::root(), index});
    }
}

TEST_CASE("the widened root covers the same leaves the input root did", "[widen]") {
    const TempDirectory directory("write_wide_nodes");
    bvh::Storage nodes(directory.path() / "input");
    const bvh::Reference root = write_balanced_tree(nodes);

    widen::SplitStorage splits(directory.path() / "splits");
    const std::array inner_row{widen::Split::keep(1)};
    const std::array root_row{
        widen::Split::keep(2),
        widen::Split::dissolve(1),
        widen::Split::dissolve(1),
    };
    write_splits(splits, inner_row, root_row);

    bvh::Storage output(directory.path() / "output");
    bvh::NodeCache cache(nodes, 4);
    const auto result = widen::write_wide_nodes(splits, cache, root, output, widen::Options{.arity = 4});
    REQUIRE(result.has_value());

    const auto input_batch = nodes.read(root.batch);
    REQUIRE(input_batch.has_value());
    const bvh::NodeHeader &expected = input_batch->nodes()[root.node].header;

    const auto output_batch = output.read(result->batch);
    REQUIRE(output_batch.has_value());
    const bvh::NodeHeader &written = output_batch->nodes()[result->node].header;

    CHECK(written.triangle_count == expected.triangle_count);
    CHECK(written.min_child_error == expected.min_child_error);
    CHECK(written.max_parent_error == expected.max_parent_error);
    CHECK(written.bounds.min == expected.bounds.min);
    CHECK(written.bounds.max == expected.bounds.max);
}

TEST_CASE("children that keep their slot stay nodes of their own", "[widen]") {
    const TempDirectory directory("write_wide_nodes");
    bvh::Storage nodes(directory.path() / "input");
    const bvh::Reference root = write_balanced_tree(nodes);

    // At arity two nothing can be widened, so every inner node keeps its slot and its row is one entry long.
    widen::SplitStorage splits(directory.path() / "splits");
    const std::array inner_row{widen::Split::keep(1)};
    const std::array root_row{widen::Split::keep(1)};
    write_splits(splits, inner_row, root_row);

    bvh::Storage output(directory.path() / "output");
    bvh::NodeCache cache(nodes, 4);
    const auto result = widen::write_wide_nodes(splits, cache, root, output, widen::Options{.arity = 2});
    REQUIRE(result.has_value());

    const std::vector<bvh::Reference> children = children_of(output, *result);
    REQUIRE(children.size() == 2);
    for (const bvh::Reference &child : children) {
        REQUIRE(children_of(output, child).size() == 2);
    }
}