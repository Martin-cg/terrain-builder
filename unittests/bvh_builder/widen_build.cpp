#include "../catch2_helpers.h"
#include "../result_helpers.h"
#include "../temp_directory.h"

#include <array>
#include <cstddef>
#include <cstdint>

#include "bvh/BatchWriter.h"
#include "bvh/Storage.h"
#include "bvh/layout.h"
#include "storage/batch_limits.h"
#include "widen/build.h"
#include "widen/options.h"

TEST_CASE("widen writes its root into the last batch", "[widen][build]") {
    const uint32_t arity = GENERATE(2u, 4u);
    const size_t max_batch_bytes = GENERATE(DEFAULT_MAX_BATCH_BYTES, size_t{1});

    const TempDirectory directory("widen_build");
    bvh::Storage input = REQUIRE_VALUE(bvh::Storage::create(directory.path() / "input"));
    bvh::BatchWriter writer(input);
    const bvh::Reference first = REQUIRE_VALUE(writer.write_leaf({}, dag::Id{octree::Id::root(), 0}));
    const bvh::Reference second = REQUIRE_VALUE(writer.write_leaf({}, dag::Id{octree::Id::root(), 1}));
    const bvh::Reference root = REQUIRE_VALUE(writer.write_inner({}, std::array{first, second}));
    REQUIRE(writer.flush().has_value());
    REQUIRE(input.save_index().has_value());

    bvh::Storage output = REQUIRE_VALUE(bvh::Storage::create(directory.path() / "output"));
    const widen::Options options{.arity = arity, .max_batch_bytes = max_batch_bytes};
    const bvh::Reference wide_root = REQUIRE_VALUE(widen::build(input, root, output, directory.path() / "scratch", options));

    CHECK(wide_root.batch == output.size() - 1);
}
