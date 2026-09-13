#include "../catch2_helpers.h"
#include "../temp_directory.h"

#include <array>
#include <limits>
#include <span>
#include <vector>

#include "enumerate.h"
#include "error/Result.h"
#include "range_utils.h"

#include "build/lbvh.h"
#include "bvh/Storage.h"
#include "bvh/utils.h"
#include "frontier/Storage.h"

namespace {

const morton::Code top_bit = morton::Code{1} << (morton::code_bits - 1);

bvh::NodeHeader header_at(const uint32_t index) {
    const double corner = index;
    return bvh::NodeHeader{
        .min_child_error = 1.0 + corner,
        .max_parent_error = 2.0 + corner,
        .triangle_count = index + 1,
        .bounds = bvh::Bounds{glm::dvec3{corner}, glm::dvec3{corner + 1.0}},
    };
}

// An entry naming no stored node, for the tests that only look at codes.
frontier::Entry entry_with(const morton::Code &code) {
    return frontier::Entry{
        .morton = code,
        .header = header_at(0),
        .ref = bvh::Reference{.batch = 0, .node = 0},
    };
}

std::vector<frontier::Entry> entries_with(const std::vector<morton::Code> &codes) {
    std::vector<frontier::Entry> entries;
    for (const morton::Code &code : codes) {
        entries.push_back(entry_with(code));
    }
    return entries;
}

// `count` codes in ascending order, spread out so that runs split at several bit depths.
std::vector<morton::Code> ascending_codes(const uint32_t count) {
    std::vector<morton::Code> codes;
    for (const uint32_t index : range(count)) {
        codes.push_back(morton::Code{index} << 7);
    }
    return codes;
}

std::vector<morton::Code> repeated_code(const uint32_t count, const morton::Code &code) {
    return std::vector<morton::Code>(count, code);
}

// One leaf node per code, all in a single batch, returned as the entries naming them.
std::vector<frontier::Entry> write_leaves(bvh::Storage &nodes, const std::vector<morton::Code> &codes) {
    bvh::Batch batch = nodes.create_batch();

    std::vector<frontier::Entry> entries;
    for (const auto &[index, code] : enumerate<uint32_t>(codes)) {
        const bvh::NodeHeader header = header_at(index);
        const bvh::NodeIndex node = batch.add_leaf_node(header, dag::Id{octree::Id::root(), index});
        entries.push_back(frontier::Entry{
            .morton = code,
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

// The tree over `entries` reaches each of them once, in order, over one inner node fewer.
void check_covers(const bvh::Storage &nodes, const frontier::Entry &root, const std::vector<frontier::Entry> &entries) {
    const Reached reached = walk_from_root(nodes, root.ref);

    REQUIRE(reached.leaves.size() == entries.size());
    REQUIRE(reached.inner_count == entries.size() - 1);

    for (const auto &[index, entry] : enumerate<size_t>(entries)) {
        REQUIRE(reached.leaves[index].batch == entry.ref.batch);
        REQUIRE(reached.leaves[index].node == entry.ref.node);
    }
}

}

TEST_CASE("the common prefix of two codes counts the leading bits they share", "[lbvh]") {
    const morton::Code zero{0};

    CHECK(lbvh::detail::count_common_prefix_bits(zero, zero) == morton::code_bits);
    CHECK(lbvh::detail::count_common_prefix_bits(zero, top_bit) == 0);
    CHECK(lbvh::detail::count_common_prefix_bits(zero, morton::Code{1}) == morton::code_bits - 1);

    for (const uint32_t bit : range(morton::code_bits)) {
        const morton::Code differing = morton::Code{1} << bit;
        REQUIRE(lbvh::detail::count_common_prefix_bits(zero, differing) == morton::code_bits - 1 - bit);
    }
}

TEST_CASE("a prefix keeps the leading bits of a code and clears the rest", "[lbvh]") {
    const morton::Code all_ones = std::numeric_limits<morton::Code>::max();

    CHECK(lbvh::detail::extract_prefix(all_ones, 0) == morton::Code{0});
    CHECK(lbvh::detail::extract_prefix(all_ones, morton::code_bits) == all_ones);
    CHECK(lbvh::detail::extract_prefix(all_ones, 1) == top_bit);
    CHECK(lbvh::detail::extract_prefix(morton::Code{1}, morton::code_bits - 1) == morton::Code{0});

    for (const uint32_t bits : range<uint32_t>(1, morton::code_bits)) {
        const morton::Code kept = all_ones << (morton::code_bits - bits);
        REQUIRE(lbvh::detail::extract_prefix(all_ones, bits) == kept);
    }
}

TEST_CASE("a run splits at the highest bit its codes disagree on", "[lbvh]") {
    const std::vector<frontier::Entry> low = entries_with({morton::Code{0}, morton::Code{1}});
    CHECK(lbvh::detail::find_split(low) == 1);

    const std::vector<frontier::Entry> high = entries_with({
        morton::Code{0},
        morton::Code{1},
        top_bit,
        top_bit | morton::Code{1},
    });
    CHECK(lbvh::detail::find_split(high) == 2);
}

TEST_CASE("a run of equal codes splits in half", "[lbvh]") {
    CHECK(lbvh::detail::find_split(entries_with(repeated_code(4, morton::Code{7}))) == 2);
    CHECK(lbvh::detail::find_split(entries_with(repeated_code(5, morton::Code{7}))) == 2);
}

TEST_CASE("a split never leaves either side empty", "[lbvh]") {
    const std::vector<std::vector<morton::Code>> runs{
        repeated_code(2, morton::Code{0}),
        repeated_code(9, morton::Code{3}),
        ascending_codes(2),
        ascending_codes(3),
        ascending_codes(16),
        ascending_codes(17),
        {morton::Code{0}, morton::Code{0}, morton::Code{1}, morton::Code{1}, morton::Code{1}},
        {morton::Code{0}, top_bit, top_bit, top_bit | morton::Code{1}},
    };

    for (const std::vector<morton::Code> &codes : runs) {
        const std::vector<frontier::Entry> entries = entries_with(codes);
        const size_t split = lbvh::detail::find_split(entries);

        REQUIRE(split > 0);
        REQUIRE(split < entries.size());
    }
}

TEST_CASE("a single entry is its own root", "[lbvh]") {
    const TempDirectory directory("lbvh_single");
    bvh::Storage nodes(directory.path() / "nodes");
    const std::vector<frontier::Entry> entries = write_leaves(nodes, ascending_codes(1));

    bvh::BatchWriter sink(nodes);
    const auto root = lbvh::detail::build_subtree(entries, sink);
    REQUIRE(root.has_value());
    REQUIRE(sink.flush().has_value());
    REQUIRE(nodes.save_index().has_value());

    CHECK(root->morton == entries.front().morton);
    CHECK(root->ref.batch == entries.front().ref.batch);
    CHECK(root->ref.node == entries.front().ref.node);
    // Nothing was written, so the leaf batch is still the only one.
    CHECK(nodes.size() == 1);
}

TEST_CASE("a pair becomes one inner node carrying both", "[lbvh]") {
    const TempDirectory directory("lbvh_pair");
    bvh::Storage nodes(directory.path() / "nodes");
    const std::vector<frontier::Entry> entries = write_leaves(nodes, ascending_codes(2));

    bvh::BatchWriter sink(nodes);
    const auto root = lbvh::detail::build_subtree(entries, sink);
    REQUIRE(root.has_value());
    REQUIRE(sink.flush().has_value());
    REQUIRE(nodes.save_index().has_value());

    const bvh::NodeHeader merged = bvh::merge_headers(std::array{entries[0].header, entries[1].header});
    CHECK(root->morton == entries.front().morton);
    CHECK(root->header.bounds == merged.bounds);
    CHECK(root->header.triangle_count == merged.triangle_count);
    CHECK(root->header.min_child_error == merged.min_child_error);
    CHECK(root->header.max_parent_error == merged.max_parent_error);

    check_covers(nodes, root.value(), entries);
}

TEST_CASE("the tree reaches every entry of a run exactly once", "[lbvh]") {
    const uint32_t count = GENERATE(2u, 3u, 5u, 8u, 33u);

    const TempDirectory directory("lbvh_covers");
    bvh::Storage nodes(directory.path() / "nodes");
    const std::vector<frontier::Entry> entries = write_leaves(nodes, ascending_codes(count));

    bvh::BatchWriter sink(nodes);
    const auto root = lbvh::detail::build_subtree(entries, sink);
    REQUIRE(root.has_value());
    REQUIRE(sink.flush().has_value());
    REQUIRE(nodes.save_index().has_value());

    check_covers(nodes, root.value(), entries);
}

TEST_CASE("a run of equal codes still builds a tree over all of them", "[lbvh]") {
    const TempDirectory directory("lbvh_equal");
    bvh::Storage nodes(directory.path() / "nodes");
    const std::vector<frontier::Entry> entries = write_leaves(nodes, repeated_code(7, morton::Code{11}));

    bvh::BatchWriter sink(nodes);
    const auto root = lbvh::detail::build_subtree(entries, sink);
    REQUIRE(root.has_value());
    REQUIRE(sink.flush().has_value());
    REQUIRE(nodes.save_index().has_value());

    check_covers(nodes, root.value(), entries);
}

TEST_CASE("the batches of a run are merged into one tree", "[lbvh]") {
    const TempDirectory directory("lbvh_batches");
    bvh::Storage nodes(directory.path() / "nodes");
    const std::vector<frontier::Entry> entries = write_leaves(nodes, ascending_codes(10));

    frontier::Storage run(directory.path() / "run");
    write_run(run, entries, {4, 1, 5});

    bvh::BatchWriter sink(nodes);
    const auto root = lbvh::build(run, sink);
    REQUIRE(root.has_value());
    REQUIRE(sink.flush().has_value());
    REQUIRE(nodes.save_index().has_value());

    CHECK(root->morton == entries.front().morton);
    check_covers(nodes, root.value(), entries);
}

TEST_CASE("building into a storage saves its index and returns the root reference", "[lbvh]") {
    const TempDirectory directory("lbvh_storage");
    bvh::Storage nodes(directory.path() / "nodes");
    const std::vector<frontier::Entry> entries = write_leaves(nodes, ascending_codes(6));

    frontier::Storage run(directory.path() / "run");
    write_run(run, entries, {3, 3});

    const auto root = lbvh::build(run, nodes, lbvh::Options{});
    REQUIRE(root.has_value());

    const bvh::Storage reopened(directory.path() / "nodes");
    CHECK(reopened.size() == nodes.size());

    const Reached reached = walk_from_root(nodes, root.value());
    CHECK(reached.leaves.size() == entries.size());
    CHECK(reached.inner_count == entries.size() - 1);
}

TEST_CASE("a small batch limit spreads the tree over several batches", "[lbvh]") {
    const TempDirectory directory("lbvh_batch_limit");
    bvh::Storage nodes(directory.path() / "nodes");
    const std::vector<frontier::Entry> entries = write_leaves(nodes, ascending_codes(8));

    frontier::Storage run(directory.path() / "run");
    write_run(run, entries, {8});

    const auto root = lbvh::build(run, nodes, lbvh::Options{.max_batch_bytes = 1});
    REQUIRE(root.has_value());

    // One batch holds the leaves, and every inner node forced a batch of its own.
    CHECK(nodes.size() == entries.size());

    const Reached reached = walk_from_root(nodes, root.value());
    CHECK(reached.leaves.size() == entries.size());
    CHECK(reached.inner_count == entries.size() - 1);
}

TEST_CASE("a batch that cannot be read is reported rather than swallowed", "[lbvh]") {
    const TempDirectory directory("lbvh_read_failure");
    bvh::Storage nodes(directory.path() / "nodes");
    const std::vector<frontier::Entry> entries = write_leaves(nodes, ascending_codes(6));

    {
        frontier::Storage run(directory.path() / "run");
        write_run(run, entries, {3, 3});
    }
    REQUIRE(std::filesystem::remove(directory.path() / "run" / "batch-00000001.bin"));

    const frontier::Storage run(directory.path() / "run");
    REQUIRE(run.size() == 2);

    bvh::BatchWriter sink(nodes);
    const auto root = lbvh::build(run, sink);
    CHECK_FALSE(root.has_value());
}
