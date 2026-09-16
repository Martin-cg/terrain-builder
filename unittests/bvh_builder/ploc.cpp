
#include "../catch2_helpers.h"
#include "../temp_directory.h"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include <glm/glm.hpp>

#include "enumerate.h"
#include "range_utils.h"

#include "build/ploc.h"
#include "bvh/BatchWriter.h"
#include "bvh/NodeCache.h"
#include "bvh/Storage.h"
#include "bvh/traverse_tree.h"
#include "frontier/Storage.h"
#include "frontier/sort.h"
#include "morton/code.h"

namespace {

const bvh::Bounds scene_bounds{glm::dvec3{0.0}, glm::dvec3{256.0}};

// Stores a leaf for each of `boxes` in `nodes` and returns the frontier over them as a single sorted run under `directory`.
frontier::Storage write_frontier(const std::filesystem::path &directory, bvh::Storage &nodes, const std::span<const bvh::Bounds> boxes) {
    bvh::BatchWriter writer(nodes);
    frontier::Batch batch;
    for (const auto &[cluster, box] : enumerate<uint32_t>(boxes)) {
        bvh::NodeHeader header{};
        header.bounds = box;
        const auto reference_result = writer.write_leaf(header, dag::Id{octree::Id::root(), cluster});
        REQUIRE(reference_result.has_value());
        batch.entries.push_back(frontier::Entry{.morton = morton::encode(scene_bounds, header), .header = header, .ref = *reference_result});
    }
    REQUIRE(writer.flush().has_value());
    REQUIRE(nodes.save_index().has_value());
    frontier::sort_entries(batch.entries);

    frontier::Storage run(directory);
    REQUIRE(run.write(batch).has_value());
    REQUIRE(run.save_index().has_value());
    return run;
}

// Options under which admission never holds a leaf back, so that only the chunking differs between them.
ploc::Options make_options(const size_t worker_count, const size_t chunk_size) {
    return ploc::Options{
        .search_radius = 2,
        .scene_bounds = scene_bounds,
        .admission = ploc::AdmissionOptions{.region_size = 8, .tolerance = 1e9},
        .chunk_size = chunk_size,
        .chunk_count = 3,
        .worker_count = worker_count,
    };
}

// The tree at `root` in pre-order, with each inner node as nothing and each leaf as its position among the leaves written.
std::vector<std::optional<uint32_t>> read_shape(const bvh::Storage &nodes, const bvh::Reference &root) {
    bvh::NodeCache cache(nodes, 1);
    std::vector<std::optional<uint32_t>> shape;
    const auto result = bvh::traverse_tree(cache, root, [&](const bvh::Reference &, const bvh::NodeView &view) {
        if (view.is_leaf()) {
            shape.push_back(view.leaf().cluster_index);
        } else {
            shape.push_back(std::nullopt);
        }
    });
    REQUIRE(result.value_or(false));
    return shape;
}

// `count` unit boxes in a row.
std::vector<bvh::Bounds> make_row(const uint32_t count) {
    return transform_vector(range(count), [](const uint32_t i) {
        const glm::dvec3 min{static_cast<double>(i), 0.0, 0.0};
        return bvh::Bounds{min, min + 1.0};
    });
}

}

TEST_CASE("a single leaf is the whole tree", "[build][ploc]") {
    const std::vector<bvh::Bounds> leaves{{glm::dvec3{0.0}, glm::dvec3{1.0}}};

    const TempDirectory directory("ploc");
    bvh::Storage nodes(directory.path() / "nodes");
    frontier::Storage frontier = write_frontier(directory.path() / "frontier", nodes, leaves);

    const auto root = ploc::build(std::move(frontier), nodes, directory.path() / "scratch", make_options(2, 8));
    REQUIRE(root.has_value());

    CHECK(read_shape(nodes, *root) == std::vector<std::optional<uint32_t>>{0});
}

TEST_CASE("neighbours are paired before distant nodes are joined", "[build][ploc]") {
    // Two pairs of touching boxes, the pairs far apart from each other.
    const std::vector<bvh::Bounds> leaves{
        {glm::dvec3{0.0}, glm::dvec3{1.0}},
        {glm::dvec3{1.0}, glm::dvec3{2.0}},
        {glm::dvec3{100.0}, glm::dvec3{101.0}},
        {glm::dvec3{101.0}, glm::dvec3{102.0}},
    };

    const TempDirectory directory("ploc");
    bvh::Storage nodes(directory.path() / "nodes");
    frontier::Storage frontier = write_frontier(directory.path() / "frontier", nodes, leaves);

    const auto root = ploc::build(std::move(frontier), nodes, directory.path() / "scratch", make_options(2, 8));
    REQUIRE(root.has_value());

    const std::vector<std::optional<uint32_t>> expected{std::nullopt, std::nullopt, 0, 1, std::nullopt, 2, 3};
    CHECK(read_shape(nodes, *root) == expected);
}

TEST_CASE("the tree holds every leaf exactly once", "[build][ploc]") {
    const uint32_t count = GENERATE(2u, 3u, 8u, 17u);

    const TempDirectory directory("ploc");
    bvh::Storage nodes(directory.path() / "nodes");
    frontier::Storage frontier = write_frontier(directory.path() / "frontier", nodes, make_row(count));

    const auto root = ploc::build(std::move(frontier), nodes, directory.path() / "scratch", make_options(2, 8));
    REQUIRE(root.has_value());

    const std::vector<std::optional<uint32_t>> shape = read_shape(nodes, *root);
    std::vector<uint32_t> clusters;
    for (const std::optional<uint32_t> &node : shape) {
        if (node.has_value()) {
            clusters.push_back(*node);
        }
    }
    std::ranges::sort(clusters);

    // Every inner node joins two subtrees, and joining count leaves into one takes count - 1 of them.
    CHECK(shape.size() - clusters.size() == count - 1);
    CHECK(clusters == to_vector(range(count)));
}

TEST_CASE("splitting the frontier into chunks does not change the tree", "[build][ploc]") {
    const std::vector<bvh::Bounds> leaves = make_row(24);

    const TempDirectory single_directory("ploc");
    bvh::Storage single_nodes(single_directory.path() / "nodes");
    frontier::Storage single_frontier = write_frontier(single_directory.path() / "frontier", single_nodes, leaves);
    const auto single_root = ploc::build(std::move(single_frontier), single_nodes, single_directory.path() / "scratch", make_options(1, 32));
    REQUIRE(single_root.has_value());

    const TempDirectory split_directory("ploc");
    bvh::Storage split_nodes(split_directory.path() / "nodes");
    frontier::Storage split_frontier = write_frontier(split_directory.path() / "frontier", split_nodes, leaves);
    const auto split_root = ploc::build(std::move(split_frontier), split_nodes, split_directory.path() / "scratch", make_options(4, 8));
    REQUIRE(split_root.has_value());

    CHECK(read_shape(split_nodes, *split_root) == read_shape(single_nodes, *single_root));
}

TEST_CASE("a frontier that cannot be read fails the build", "[build][ploc]") {
    const TempDirectory directory("ploc");
    bvh::Storage nodes(directory.path() / "nodes");
    frontier::Storage frontier = write_frontier(directory.path() / "frontier", nodes, make_row(4));

    // The storage still counts its batch, but it can no longer be read.
    std::filesystem::remove_all(directory.path() / "frontier");

    const auto root = ploc::build(std::move(frontier), nodes, directory.path() / "scratch", make_options(2, 8));
    CHECK_FALSE(root.has_value());
}
