#include "../catch2_helpers.h"
#include "../temp_directory.h"

#include <span>
#include <vector>

#include "enumerate.h"
#include "error/Result.h"
#include "range_utils.h"

#include "build/ploc.h"
#include "bvh/Storage.h"
#include "frontier/Storage.h"

namespace {

const bvh::Bounds scene_bounds{glm::dvec3{0.0}, glm::dvec3{256.0}};

// Nodes of equal size, so that every region admits all of them.
bvh::NodeHeader header_at(const uint32_t index) {
    const double corner = index;
    return bvh::NodeHeader{
        .min_child_error = 1.0,
        .max_parent_error = 2.0,
        .triangle_count = index + 1,
        .bounds = bvh::Bounds{glm::dvec3{corner}, glm::dvec3{corner + 1.0}},
    };
}

// One leaf node per entry, all in a single batch, in ascending morton order.
std::vector<frontier::Entry> write_leaves(bvh::Storage &nodes, const uint32_t count) {
    bvh::Batch batch = nodes.create_batch();

    std::vector<frontier::Entry> entries;
    for (const uint32_t index : range(count)) {
        const bvh::NodeHeader header = header_at(index);
        const bvh::NodeIndex node = batch.add_leaf_node(header, dag::Id{octree::Id::root(), index});
        entries.push_back(frontier::Entry{
            .morton = morton::Code{index} << 7,
            .header = header,
            .ref = bvh::Reference{.batch = batch.index(), .node = node},
        });
    }

    REQUIRE(nodes.write(batch).has_value());
    REQUIRE(nodes.save_index().has_value());
    return entries;
}

// `entries_per_batch[i]` entries in batch i, taken in order.
void write_run(frontier::Storage &run, const std::vector<frontier::Entry> &entries, const std::vector<uint32_t> &entries_per_batch) {
    std::span<const frontier::Entry> remaining(entries);
    for (const uint32_t count : entries_per_batch) {
        REQUIRE(count <= remaining.size());

        frontier::Batch batch;
        batch.entries.assign_range(remaining.first(count));
        REQUIRE(run.write(batch).has_value());

        remaining = remaining.subspan(count);
    }

    REQUIRE(remaining.empty());
    REQUIRE(run.save_index().has_value());
}

ploc::Options options_for(const size_t worker_count, const size_t chunk_size) {
    return ploc::Options{
        .search_radius = 2,
        .scene_bounds = scene_bounds,
        .admission = ploc::AdmissionOptions{.region_size = 8, .tolerance = 1e9},
        .chunk_size = chunk_size,
        .chunk_count = 3,
        .worker_count = worker_count,
    };
}

std::vector<bvh::Batch> read_batches(const bvh::Storage &nodes) {
    std::vector<bvh::Batch> batches;
    for (const uint32_t index : range(nodes.size())) {
        alp::Result<bvh::Batch, io::Error> batch = nodes.read(index);
        REQUIRE(batch.has_value());
        batches.push_back(std::move(batch).value());
    }
    return batches;
}

// What a walk from a root node arrives at.
struct Reached {
    std::vector<bvh::Reference> leaves;
    uint32_t inner_count = 0;
};

void walk_from(const std::vector<bvh::Batch> &batches, const bvh::Reference &ref, Reached &reached) {
    REQUIRE(ref.batch < batches.size());
    const bvh::Batch &batch = batches[ref.batch];

    REQUIRE(ref.node < batch.nodes().size());
    const bvh::Node &node = batch.nodes()[ref.node];

    if (node.is_leaf()) {
        reached.leaves.push_back(ref);
        return;
    }

    reached.inner_count++;
    for (const bvh::Reference &child : batch.children().subspan(node.child_start, node.child_count)) {
        walk_from(batches, child, reached);
    }
}

Reached walk_from_root(const bvh::Storage &nodes, const bvh::Reference &root) {
    const std::vector<bvh::Batch> batches = read_batches(nodes);

    Reached reached;
    walk_from(batches, root, reached);
    return reached;
}

// The leaves a build over `count` entries reaches, which is the tree it agreed on.
std::vector<bvh::Reference> build_and_walk(
    const TempDirectory &directory, const uint32_t count, const ploc::Options &options, uint32_t &inner_count) {
    bvh::Storage nodes(directory.path() / "nodes");
    const std::vector<frontier::Entry> entries = write_leaves(nodes, count);

    frontier::Storage run(directory.path() / "run");
    write_run(run, entries, {count});

    const auto root = ploc::build(std::move(run), nodes, directory.path() / "scratch", options);
    REQUIRE(root.has_value());

    const Reached reached = walk_from_root(nodes, root.value());
    REQUIRE(reached.leaves.size() == entries.size());

    inner_count = reached.inner_count;
    return reached.leaves;
}

}

TEST_CASE("a single entry is already the root of the frontier", "[ploc]") {
    const TempDirectory directory("ploc_single");
    bvh::Storage nodes(directory.path() / "nodes");
    const std::vector<frontier::Entry> entries = write_leaves(nodes, 1);

    frontier::Storage run(directory.path() / "run");
    write_run(run, entries, {1});

    const auto root = ploc::build(std::move(run), nodes, directory.path() / "scratch", options_for(2, 8));
    REQUIRE(root.has_value());

    CHECK(root->batch == entries.front().ref.batch);
    CHECK(root->node == entries.front().ref.node);
}

TEST_CASE("the tree reaches every entry of the frontier exactly once", "[ploc]") {
    const uint32_t count = GENERATE(2u, 3u, 8u, 17u);

    const TempDirectory directory("ploc_covers");
    uint32_t inner_count = 0;
    const std::vector<bvh::Reference> leaves = build_and_walk(directory, count, options_for(2, 8), inner_count);

    // Every merge adds one inner node, and merging down to one root takes count - 1 of them.
    CHECK(leaves.size() == count);
    CHECK(inner_count == count - 1);
}

TEST_CASE("the halo makes chunking invisible to the result", "[ploc]") {
    const uint32_t count = 24;

    const TempDirectory single_directory("ploc_halo_single");
    uint32_t single_inner = 0;
    const std::vector<bvh::Reference> single = build_and_walk(single_directory, count, options_for(1, 32), single_inner);

    const TempDirectory split_directory("ploc_halo_split");
    uint32_t split_inner = 0;
    const std::vector<bvh::Reference> split = build_and_walk(split_directory, count, options_for(4, 8), split_inner);

    CHECK(split_inner == single_inner);
    CHECK(split == single);
}

TEST_CASE("a batch that cannot be read is reported rather than swallowed", "[ploc]") {
    const TempDirectory directory("ploc_read_failure");
    bvh::Storage nodes(directory.path() / "nodes");
    const std::vector<frontier::Entry> entries = write_leaves(nodes, 12);

    {
        frontier::Storage run(directory.path() / "run");
        write_run(run, entries, {6, 6});
    }
    REQUIRE(std::filesystem::remove(directory.path() / "run" / "batch-00000001.bin"));

    frontier::Storage run(directory.path() / "run");
    REQUIRE(run.size() == 2);

    const auto root = ploc::build(std::move(run), nodes, directory.path() / "scratch", options_for(2, 8));
    CHECK_FALSE(root.has_value());
}
