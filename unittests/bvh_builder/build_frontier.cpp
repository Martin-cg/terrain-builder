#include "../catch2_helpers.h"
#include "../temp_directory.h"
#include "build_frontier.h"

#include <vector>

#include "bvh_utils.h"
#include "storage.h"
#include "traverse.h"

namespace {

const bvh::Bounds scene{glm::dvec3{0.0}, glm::dvec3{1.0}};

bvh::Bounds cube_at(const glm::dvec3 &centre, const double size) {
    return bvh::Bounds{centre - size * 0.5, centre + size * 0.5};
}

bvh::NodeHeader header_at(const bvh::Bounds &bounds, const uint32_t triangle_count) {
    return bvh::NodeHeader{
        .min_child_error = 1.0,
        .max_parent_error = 2.0,
        .triangle_count = triangle_count,
        .bounds = bounds,
    };
}

// One leaf per bounds, all in a single batch.
void write_nodes(bvh::Storage &nodes, const std::vector<bvh::Bounds> &bounds) {
    bvh::Batch batch = nodes.create_batch();
    for (const auto &[index, node_bounds] : enumerate<uint32_t>(bounds)) {
        emit_leaf_node(batch, header_at(node_bounds, index), dag::Id{octree::Id::root(), index});
    }
    REQUIRE(nodes.write(batch).has_value());
    REQUIRE(nodes.save().has_value());
}

std::vector<frontier::Entry> read_run(const frontier::RunStorage &run) {
    std::vector<frontier::Entry> entries;
    const auto result = frontier::traverse(run, [&](const frontier::Entry &entry) { entries.push_back(entry); });
    REQUIRE(result.has_value());
    return entries;
}

}

TEST_CASE("every stored node enters the frontier carrying its header and reference", "[frontier]") {
    const TempDirectory directory("build_frontier");
    std::filesystem::create_directories(directory.path() / "nodes");
    bvh::Storage nodes(directory.path() / "nodes");
    write_nodes(nodes, {cube_at(glm::dvec3{0.25}, 0.1), cube_at(glm::dvec3{0.75}, 0.1)});

    std::filesystem::create_directories(directory.path() / "frontier");
    frontier::RunStorage run(directory.path() / "frontier");
    REQUIRE(frontier::build_from_leaves(nodes, scene, run).has_value());
    REQUIRE(run.save().has_value());

    const std::vector<frontier::Entry> entries = read_run(run);
    REQUIRE(entries.size() == 2);
    for (const auto &[index, entry] : enumerate<uint32_t>(entries)) {
        CHECK(entry.ref.batch == 0);
        CHECK(entry.ref.node == index);
        CHECK(entry.header.triangle_count == index);
        CHECK(entry.header.min_child_error == 1.0);
        CHECK(entry.header.max_parent_error == 2.0);
    }
    CHECK(entries[0].morton < entries[1].morton);
}

TEST_CASE("the entry's morton code comes from the bounds centre", "[frontier]") {
    const TempDirectory directory("build_frontier");
    std::filesystem::create_directories(directory.path() / "nodes");
    bvh::Storage nodes(directory.path() / "nodes");

    // Same centre, different extent, so only a code taken at the centre makes these agree.
    write_nodes(nodes, {
        cube_at(glm::dvec3{0.25}, 0.1),
        cube_at(glm::dvec3{0.25}, 0.4),
        cube_at(glm::dvec3{0.75}, 0.1),
    });

    std::filesystem::create_directories(directory.path() / "frontier");
    frontier::RunStorage run(directory.path() / "frontier");
    REQUIRE(frontier::build_from_leaves(nodes, scene, run).has_value());
    REQUIRE(run.save().has_value());

    const std::vector<frontier::Entry> entries = read_run(run);
    REQUIRE(entries.size() == 3);
    CHECK(entries[0].morton == entries[1].morton);
    CHECK(entries[0].morton != entries[2].morton);
}

TEST_CASE("the byte budget splits the frontier into batches", "[frontier]") {
    const TempDirectory directory("build_frontier");
    std::filesystem::create_directories(directory.path() / "nodes");
    bvh::Storage nodes(directory.path() / "nodes");

    std::vector<bvh::Bounds> bounds;
    for (uint32_t i = 0; i < 5; i++) {
        bounds.push_back(cube_at(glm::dvec3{0.1 * (i + 1)}, 0.05));
    }
    write_nodes(nodes, bounds);

    std::filesystem::create_directories(directory.path() / "frontier");
    frontier::RunStorage run(directory.path() / "frontier");
    const frontier::BuildOptions options{.max_batch_bytes = 2 * sizeof(frontier::Entry)};
    REQUIRE(frontier::build_from_leaves(nodes, scene, run, options).has_value());
    REQUIRE(run.save().has_value());

    CHECK(run.size() == 3);
    CHECK(read_run(run).size() == 5);
}

TEST_CASE("an empty store yields an empty frontier", "[frontier]") {
    const TempDirectory directory("build_frontier");
    std::filesystem::create_directories(directory.path() / "nodes");
    const bvh::Storage nodes(directory.path() / "nodes");

    std::filesystem::create_directories(directory.path() / "frontier");
    frontier::RunStorage run(directory.path() / "frontier");
    REQUIRE(frontier::build_from_leaves(nodes, scene, run).has_value());
    REQUIRE(run.save().has_value());

    CHECK(run.size() == 0);
}