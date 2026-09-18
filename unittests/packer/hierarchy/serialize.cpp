#include <utility>
#include <vector>

#include "../../catch2_helpers.h"
#include "bvh/layout.h"
#include "hierarchy/build.h"
#include "hierarchy/reader.h"
#include "hierarchy/serialize.h"
#include "io/envelope.h"

namespace packer {

TEST_CASE("hierarchy::deserialize reads back the page hierarchy::serialize wrote", "[hierarchy][serialize]") {
    const bvh::Bounds bounds{.min = {0.0, 0.0, 0.0}, .max = {1.0, 1.0, 1.0}};
    const bvh::NodeHeader inner{.min_child_error = 1.0, .max_parent_error = 4.0, .bounds = bounds};
    const bvh::NodeHeader leaf{.min_child_error = 0.0, .max_parent_error = 1.0, .bounds = bounds};

    // The root holds one child of each kind, and its local child holds a leaf.
    hierarchy::PageBuilder builder;
    hierarchy::NodeWriter root = builder.push_root(bounds);
    root.push_local(inner).push_leaf(leaf, {.page = PageId{1}, .cluster = 4, .texture_page = PageId{2}, .texture = 5});
    root.push_leaf(leaf, {.page = PageId{1}, .cluster = 6, .texture_page = PageId{2}, .texture = 7});
    root.push_external(inner, {.page = PageId{3}, .root = 8});
    const hierarchy::EncodedPage page = std::move(builder).finish();

    hierarchy::PageReader original(page);
    const std::vector<std::vector<hierarchy::Child>> expected = original.read_all_nodes();
    REQUIRE(expected.size() == 2);

    const auto bytes_result = hierarchy::serialize(page);
    REQUIRE(bytes_result.has_value());
    auto reader_result = hierarchy::deserialize(*bytes_result);
    REQUIRE(reader_result.has_value());

    CHECK(reader_result->read_all_nodes() == expected);
}

TEST_CASE("hierarchy::deserialize passes an envelope failure through", "[hierarchy][serialize]") {
    using ForeignSchema = io::envelope::PayloadSchema<"OtherPage", io::envelope::Version<1, hierarchy::EncodedPage>>;

    const auto bytes_result = io::envelope::serialize<ForeignSchema, 1>(hierarchy::EncodedPage{});
    REQUIRE(bytes_result.has_value());

    const auto result = hierarchy::deserialize(*bytes_result);
    REQUIRE(!result.has_value());
    CHECK(result.error().code == io::envelope::ErrorCode::WrongClassName);
}

}
