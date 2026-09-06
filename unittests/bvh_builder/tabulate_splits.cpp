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

namespace {

// A leaf covering the unit box shifted along x, so that merging siblings costs more than either of them.
bvh::NodeHeader leaf_header(const uint32_t index) {
    const auto offset = static_cast<double>(index);
    return bvh::NodeHeader{
        .min_child_error = 1.0,
        .max_parent_error = 2.0,
        .triangle_count = index,
        .bounds = bvh::Bounds{glm::dvec3{offset, 0.0, 0.0}, glm::dvec3{offset + 1.0, 1.0, 1.0}},
    };
}

// The tree R(A(0, 1), B(2, 3)), with the leaves, their parents and the root each in a batch of their own.
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

// Every row of `splits`, in the order it was written.
std::vector<std::vector<widen::Split>> collect_rows(const widen::SplitStorage &splits) {
    std::vector<std::vector<widen::Split>> rows;
    for (uint32_t index = 0; index < splits.size(); index++) {
        const auto result = splits.read(index);
        REQUIRE(result.has_value());
        for (const std::span<const widen::Split> row : result->splits) {
            rows.emplace_back(row.begin(), row.end());
        }
    }
    return rows;
}

// Tabulates the balanced tree and hands back its rows.
std::vector<std::vector<widen::Split>> tabulate(const TempDirectory &directory, const uint32_t arity) {
    bvh::Storage nodes(directory.path() / "input");
    const bvh::Reference root = write_balanced_tree(nodes);

    widen::SplitStorage splits(directory.path() / "splits");
    bvh::NodeCache cache(nodes, 4);
    REQUIRE(widen::tabulate_splits(cache, root, splits, widen::Options{.arity = arity}).has_value());
    REQUIRE(splits.save_index().has_value());

    return collect_rows(splits);
}

}

TEST_CASE("every inner node is tabulated once", "[widen]") {
    const TempDirectory directory("tabulate_splits");

    CHECK(tabulate(directory, 4).size() == 3);
}

TEST_CASE("a node offered one slot has to keep it", "[widen]") {
    const TempDirectory directory("tabulate_splits");

    const std::vector<std::vector<widen::Split>> rows = tabulate(directory, 4);
    REQUIRE(!rows.empty());
    for (const std::vector<widen::Split> &row : rows) {
        REQUIRE(!row.empty());
        REQUIRE(row.front().is_kept());
    }
}

TEST_CASE("a node with room for every leaf below it is never kept", "[widen]") {
    const TempDirectory directory("tabulate_splits");

    // Four leaves under the root, so at four slots each of them can have one and nothing needs keeping.
    const std::vector<std::vector<widen::Split>> rows = tabulate(directory, 4);
    REQUIRE(rows.size() == 3);

    const std::vector<widen::Split> &root_row = rows.back();
    REQUIRE(root_row.size() >= 2);
    CHECK(!root_row.back().is_kept());
}