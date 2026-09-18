#include <catch2/catch_test_macros.hpp>

#include <system_error>

#include <fmt/format.h>

#include "../temp_directory.h"
#include "error/format.h"
#include "io/FileError.h"
#include "octree/Id.h"
#include "octree/storage/MeshStorage.h"
#include "octree/storage/open.h"

TEST_CASE("Loading a node the index does not list fails as a missing file, naming the node", "[octree][storage]") {
    const TempDirectory directory("octree_storage_index_miss");
    const octree::IndexedMeshStorage storage = octree::open_folder_indexed(directory.path());
    const octree::Id id = octree::Id::root();

    const auto result = storage.load(id);

    REQUIRE(!result.has_value());
    CHECK(result.error().holds(io::FileError(std::make_error_code(std::errc::no_such_file_or_directory), storage.path_for(id))));
    CHECK(fmt::to_string(result.error()).contains(fmt::to_string(id)));
}
