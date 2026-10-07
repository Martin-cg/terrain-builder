#include "../catch2_helpers.h"
#include "../result_helpers.h"
#include "../temp_directory.h"

#include <array>
#include <optional>

#include "bvh/BatchWriter.h"
#include "bvh/Storage.h"
#include "bvh/find_root.h"
#include "bvh/layout.h"

TEST_CASE("find_root finds the root of a tree in a batch", "[bvh][find_root]") {
    bvh::Batch batch(7);
    const bvh::NodeIndex first = batch.add_leaf_node({}, dag::Id{octree::Id::root(), 0});
    const bvh::NodeIndex second = batch.add_leaf_node({}, dag::Id{octree::Id::root(), 1});
    batch.add_inner_node({}, std::array{bvh::Reference{.batch = 7, .node = first}, bvh::Reference{.batch = 7, .node = second}});

    CHECK(bvh::find_root(batch) == bvh::Reference{.batch = 7, .node = 2});
}

TEST_CASE("find_root finds a root that is not the last node of its batch", "[bvh][find_root]") {
    // The root comes first and references the two leaves stored after it.
    bvh::Batch batch(7);
    batch.add_inner_node({}, std::array{bvh::Reference{.batch = 7, .node = 1}, bvh::Reference{.batch = 7, .node = 2}});
    batch.add_leaf_node({}, dag::Id{octree::Id::root(), 0});
    batch.add_leaf_node({}, dag::Id{octree::Id::root(), 1});

    CHECK(bvh::find_root(batch) == bvh::Reference{.batch = 7, .node = 0});
}

TEST_CASE("find_root takes a lone leaf as its own root", "[bvh][find_root]") {
    bvh::Batch batch(7);
    batch.add_leaf_node({}, dag::Id{octree::Id::root(), 0});

    CHECK(bvh::find_root(batch) == bvh::Reference{.batch = 7, .node = 0});
}

TEST_CASE("find_root finds nothing in a batch holding two trees", "[bvh][find_root]") {
    bvh::Batch batch(7);
    batch.add_leaf_node({}, dag::Id{octree::Id::root(), 0});
    batch.add_leaf_node({}, dag::Id{octree::Id::root(), 1});
    batch.add_inner_node({}, std::array{bvh::Reference{.batch = 7, .node = 0}});
    batch.add_inner_node({}, std::array{bvh::Reference{.batch = 7, .node = 1}});

    CHECK(bvh::find_root(batch) == std::nullopt);
}

TEST_CASE("find_root finds nothing in an empty batch", "[bvh][find_root]") {
    const bvh::Batch batch(7);

    CHECK(bvh::find_root(batch) == std::nullopt);
}

TEST_CASE("find_root finds the root of a storage in its last batch", "[bvh][find_root]") {
    const TempDirectory directory("find_root");
    bvh::Storage nodes = REQUIRE_VALUE(bvh::Storage::create(directory.path()));
    // One node per batch, so that every node but the root lies outside the last batch.
    bvh::BatchWriter writer(nodes, 1);
    const bvh::Reference first = REQUIRE_VALUE(writer.write_leaf({}, dag::Id{octree::Id::root(), 0}));
    const bvh::Reference second = REQUIRE_VALUE(writer.write_leaf({}, dag::Id{octree::Id::root(), 1}));
    const bvh::Reference root = REQUIRE_VALUE(writer.write_inner({}, std::array{first, second}));
    REQUIRE(writer.flush().has_value());
    REQUIRE(nodes.save_index().has_value());

    CHECK(REQUIRE_VALUE(bvh::find_root(nodes)) == root);
}
