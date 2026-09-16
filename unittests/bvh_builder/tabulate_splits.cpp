#include "../catch2_helpers.h"
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

namespace {

// Writes the tree R(A(0, 1), B(2, 3)) over a row of four unit boxes into `nodes` and returns its root.
bvh::Reference write_balanced_tree(bvh::Storage &nodes) {
    bvh::BatchWriter writer(nodes);
    const auto write_leaf = [&](const uint32_t cluster, const bvh::Bounds &bounds) {
        const auto result = writer.write_leaf(bvh::NodeHeader{.min_child_error = 0.0, .max_parent_error = 0.0, .bounds = bounds}, dag::Id{octree::Id::root(), cluster});
        REQUIRE(result.has_value());
        return *result;
    };
    const auto write_inner = [&](const bvh::Bounds &bounds, const std::array<bvh::Reference, 2> &children) {
        const auto result = writer.write_inner(bvh::NodeHeader{.min_child_error = 0.0, .max_parent_error = 0.0, .bounds = bounds}, children);
        REQUIRE(result.has_value());
        return *result;
    };

    const bvh::Reference leaf_0 = write_leaf(0, {glm::dvec3{0.0, 0.0, 0.0}, glm::dvec3{1.0, 1.0, 1.0}});
    const bvh::Reference leaf_1 = write_leaf(1, {glm::dvec3{1.0, 0.0, 0.0}, glm::dvec3{2.0, 1.0, 1.0}});
    const bvh::Reference leaf_2 = write_leaf(2, {glm::dvec3{2.0, 0.0, 0.0}, glm::dvec3{3.0, 1.0, 1.0}});
    const bvh::Reference leaf_3 = write_leaf(3, {glm::dvec3{3.0, 0.0, 0.0}, glm::dvec3{4.0, 1.0, 1.0}});
    const bvh::Reference a = write_inner({glm::dvec3{0.0, 0.0, 0.0}, glm::dvec3{2.0, 1.0, 1.0}}, {leaf_0, leaf_1});
    const bvh::Reference b = write_inner({glm::dvec3{2.0, 0.0, 0.0}, glm::dvec3{4.0, 1.0, 1.0}}, {leaf_2, leaf_3});
    const bvh::Reference root = write_inner({glm::dvec3{0.0, 0.0, 0.0}, glm::dvec3{4.0, 1.0, 1.0}}, {a, b});

    REQUIRE(writer.flush().has_value());
    REQUIRE(nodes.save_index().has_value());
    return root;
}

// Tabulates the balanced tree at `arity` and returns its rows in the order they were written.
std::vector<std::vector<widen::Split>> tabulate(const TempDirectory &directory, const uint32_t arity) {
    bvh::Storage nodes(directory.path() / "nodes");
    const bvh::Reference root = write_balanced_tree(nodes);

    widen::SplitStorage splits(directory.path() / "splits");
    bvh::NodeCache cache(nodes, 1);
    REQUIRE(widen::tabulate_splits(cache, root, splits, widen::Options{.arity = arity}).has_value());
    REQUIRE(splits.save_index().has_value());

    std::vector<std::vector<widen::Split>> rows;
    for (uint32_t index = 0; index < splits.size(); index++) {
        const auto batch = splits.read(index);
        REQUIRE(batch.has_value());
        for (const std::span<const widen::Split> row : batch->splits) {
            rows.emplace_back(row.begin(), row.end());
        }
    }
    return rows;
}

}

TEST_CASE("every inner node gets one row, the root's last", "[tabulate_splits]") {
    const TempDirectory directory("tabulate_splits");

    const std::vector<std::vector<widen::Split>> rows = tabulate(directory, 4);

    // A and B have one row each, and only the root is offered more than one slot.
    REQUIRE(rows.size() == 3);
    CHECK(rows[0].size() == 1);
    CHECK(rows[1].size() == 1);
    CHECK(rows[2].size() > 1);
}

TEST_CASE("a node over two leaves keeps itself and gives each leaf a slot", "[tabulate_splits]") {
    const TempDirectory directory("tabulate_splits");

    const std::vector<std::vector<widen::Split>> rows = tabulate(directory, 4);

    // Leaves cost nothing however many slots they get, so the first split that seats both wins.
    REQUIRE(rows.size() == 3);
    CHECK(rows[0] == std::vector{widen::Split::keep(1)});
    CHECK(rows[1] == std::vector{widen::Split::keep(1)});
}

TEST_CASE("a node offered a single slot has to keep itself", "[tabulate_splits]") {
    const TempDirectory directory("tabulate_splits");

    for (const std::vector<widen::Split> &row : tabulate(directory, 4)) {
        REQUIRE(!row.empty());
        CHECK(row.front().is_kept());
    }
}

TEST_CASE("the root dissolves once its children can use the slots more cheaply than it can", "[tabulate_splits]") {
    const TempDirectory directory("tabulate_splits");

    const std::vector<std::vector<widen::Split>> rows = tabulate(directory, 4);

    // At three slots A keeps one and B's leaves take the other two, which costs A alone and so less than the whole root.
    REQUIRE(rows.size() == 3);
    const std::vector<widen::Split> &root_row = rows[2];
    REQUIRE(root_row.size() == 3);
    CHECK(root_row[2] == widen::Split::dissolve(1));
}