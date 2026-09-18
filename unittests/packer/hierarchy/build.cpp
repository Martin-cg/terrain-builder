#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "../../catch2_helpers.h"
#include "bvh/layout.h"
#include "hierarchy/build.h"
#include "hierarchy/reader.h"
#include "numeric/checked_cast.h"

namespace packer {

TEST_CASE("hierarchy::PageBuilder writes a page that reads back as the pushed tree", "[hierarchy][build]") {
    const PageId geometry_page{1};
    const PageId texture_page{2};
    const PageId other_page{7};

    const bvh::Bounds r0_bounds{.min = {0.0, 0.0, 0.0}, .max = {100.0, 100.0, 100.0}};
    const bvh::Bounds r1_bounds{.min = {200.0, 0.0, 0.0}, .max = {300.0, 100.0, 100.0}};
    const bvh::NodeHeader a{.min_child_error = 1.0, .max_parent_error = 4.0, .bounds = {.min = {10.0, 10.0, 10.0}, .max = {60.0, 60.0, 60.0}}};
    const bvh::NodeHeader l1{.min_child_error = 0.5, .max_parent_error = 1.0, .bounds = {.min = {60.0, 0.0, 0.0}, .max = {100.0, 50.0, 50.0}}};
    const bvh::NodeHeader x{.min_child_error = 2.0, .max_parent_error = 8.0, .bounds = {.min = {210.0, 10.0, 10.0}, .max = {290.0, 90.0, 90.0}}};
    const bvh::NodeHeader l2{.min_child_error = 0.25, .max_parent_error = 1.0, .bounds = {.min = {20.0, 20.0, 20.0}, .max = {30.0, 30.0, 30.0}}};

    // Both leaves share a geometry page and a texture page.
    const hierarchy::LeafChild l1_target{.page = geometry_page, .cluster = 5, .texture_page = texture_page, .texture = 0};
    const hierarchy::LeafChild l2_target{.page = geometry_page, .cluster = 6, .texture_page = texture_page, .texture = 1};
    const hierarchy::ExternalChild x_target{.page = other_page, .root = 3};

    // R0 -> A, L1
    // R1 -> X
    // A  -> L2
    hierarchy::PageBuilder builder;
    hierarchy::NodeWriter r0 = builder.push_root(r0_bounds);
    hierarchy::NodeWriter a_writer = r0.push_local(a);
    a_writer.push_leaf(l2, l2_target);
    r0.push_leaf(l1, l1_target);
    hierarchy::NodeWriter r1 = builder.push_root(r1_bounds);
    r1.push_external(x, x_target);

    hierarchy::PageReader reader(std::move(builder).finish());
    const auto read_node = [&reader] {
        const std::optional<std::span<const hierarchy::Child>> node_result = reader.read_next_node();
        REQUIRE(node_result.has_value());
        return std::vector<hierarchy::Child>(node_result->begin(), node_result->end());
    };
    const auto contains = [](const GridBox &box, const bvh::Bounds &bounds) {
        const GridBox quantized = quantize_bounds(bounds);
        return box.contains_inclusive(quantized.min) && box.contains_inclusive(quantized.max);
    };

    // Nodes come back in page order: the roots, then A.
    const std::vector<hierarchy::Child> r0_children = read_node();
    const std::vector<hierarchy::Child> r1_children = read_node();
    const std::vector<hierarchy::Child> a_children = read_node();
    CHECK(!reader.read_next_node().has_value());

    REQUIRE(r0_children.size() == 2);

    CHECK(contains(r0_children[0].box, a.bounds));
    CHECK(r0_children[0].min_error == 1.0f);
    CHECK(r0_children[0].max_parent_error == 4.0f);
    CHECK(r0_children[0].target == hierarchy::Child::Target{hierarchy::LocalChild{.node = 2}});

    CHECK(contains(r0_children[1].box, l1.bounds));
    CHECK(r0_children[1].min_error == 0.5f);
    CHECK(r0_children[1].max_parent_error == 1.0f);
    CHECK(r0_children[1].target == hierarchy::Child::Target{l1_target});

    REQUIRE(r1_children.size() == 1);

    CHECK(contains(r1_children[0].box, x.bounds));
    CHECK(r1_children[0].min_error == 2.0f);
    CHECK(r1_children[0].max_parent_error == 8.0f);
    CHECK(r1_children[0].target == hierarchy::Child::Target{x_target});

    REQUIRE(a_children.size() == 1);

    CHECK(contains(a_children[0].box, l2.bounds));
    CHECK(a_children[0].min_error == 0.25f);
    CHECK(a_children[0].max_parent_error == 1.0f);
    CHECK(a_children[0].target == hierarchy::Child::Target{l2_target});
}

TEST_CASE("hierarchy::PageBuilder rejects a leaf with an inverted error interval", "[hierarchy][build]") {
    const bvh::Bounds bounds{.min = {0.0, 0.0, 0.0}, .max = {1.0, 1.0, 1.0}};
    const bvh::NodeHeader inverted{.min_child_error = 2.0, .max_parent_error = 1.0, .bounds = bounds};
    const hierarchy::LeafChild target{.page = PageId{1}, .cluster = 0, .texture_page = PageId{2}, .texture = 0};

    CHECK_THROWS([&] {
        hierarchy::PageBuilder builder;
        builder.push_root(bounds).push_leaf(inverted, target);
        return std::move(builder).finish();
    }());
}

TEST_CASE("hierarchy::PageBuilder rejects a node with more children than a page can count", "[hierarchy][build]") {
    const bvh::Bounds bounds{.min = {0.0, 0.0, 0.0}, .max = {1.0, 1.0, 1.0}};
    const bvh::NodeHeader leaf{.min_child_error = 0.0, .max_parent_error = 1.0, .bounds = bounds};
    const hierarchy::LeafChild target{.page = PageId{1}, .cluster = 0, .texture_page = PageId{2}, .texture = 0};

    CHECK_THROWS([&] {
        hierarchy::PageBuilder builder;
        hierarchy::NodeWriter root = builder.push_root(bounds);
        for (uint32_t i = 0; i < 256; i++) {
            root.push_leaf(leaf, target);
        }
        return std::move(builder).finish();
    }());
}

TEST_CASE("hierarchy::PageBuilder rejects more nodes than a page can hold", "[hierarchy][build]") {
    const bvh::Bounds bounds{.min = {0.0, 0.0, 0.0}, .max = {1.0, 1.0, 1.0}};
    const bvh::NodeHeader inner{.min_child_error = 1.0, .max_parent_error = 4.0, .bounds = bounds};

    // A chain of local nodes below one root, one node too many.
    CHECK_THROWS([&] {
        hierarchy::PageBuilder builder;
        hierarchy::NodeWriter node = builder.push_root(bounds);
        for (uint32_t i = 0; i < 65535; i++) {
            node = node.push_local(inner);
        }
        return std::move(builder).finish();
    }());
}

TEST_CASE("hierarchy::PageBuilder rejects more referenced pages than a page can hold", "[hierarchy][build]") {
    const bvh::Bounds bounds{.min = {0.0, 0.0, 0.0}, .max = {1.0, 1.0, 1.0}};
    const bvh::NodeHeader inner{.min_child_error = 1.0, .max_parent_error = 4.0, .bounds = bounds};
    const bvh::NodeHeader leaf{.min_child_error = 0.0, .max_parent_error = 1.0, .bounds = bounds};

    // Leaves in 65536 distinct pages, spread over a chain of local nodes so no node exceeds its child limit.
    CHECK_THROWS([&] {
        hierarchy::PageBuilder builder;
        hierarchy::NodeWriter node = builder.push_root(bounds);
        for (uint32_t i = 0; i < 65536; i++) {
            if (i > 0 && i % 254 == 0) {
                node = node.push_local(inner);
            }
            const PageId page{checked_cast<uint8_t>(i & 0xff), checked_cast<uint8_t>(i >> 8), 1};
            node.push_leaf(leaf, {.page = page, .cluster = 0, .texture_page = page, .texture = 0});
        }
        return std::move(builder).finish();
    }());
}

}
