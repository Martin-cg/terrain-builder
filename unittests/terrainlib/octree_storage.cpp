#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

#include <fmt/format.h>

#include "../temp_directory.h"
#include "error/format.h"
#include "io/FileError.h"
#include "io/bytes.h"
#include "octree/Id.h"
#include "octree/storage/MeshStorage.h"
#include "octree/storage/open.h"

TEST_CASE("Loading a node the index does not list fails as a missing file", "[octree][storage]") {
    const TempDirectory directory("octree_storage_index_miss");
    const octree::IndexedMeshStorage storage = octree::open_folder_indexed(directory.path());
    const octree::Id id = octree::Id::root();

    const auto result = storage.load(id);

    REQUIRE(!result.has_value());
    CHECK(result.error().holds(io::FileError(std::make_error_code(std::errc::no_such_file_or_directory), storage.path_for(id))));
}

TEST_CASE("A failed node load names the node", "[octree][storage]") {
    const TempDirectory directory("octree_storage_failed_load");
    const octree::Id id = octree::Id::root();
    std::string message;

    SECTION("missing from the index") {
        const octree::IndexedMeshStorage storage = octree::open_folder_indexed(directory.path());
        const auto result = storage.load(id);
        REQUIRE(!result.has_value());
        message = fmt::to_string(result.error());
    }

    SECTION("file missing from an unindexed storage") {
        const octree::MeshStorage storage = octree::open_folder(directory.path());
        const auto result = storage.load(id);
        REQUIRE(!result.has_value());
        message = fmt::to_string(result.error());
    }

    CHECK(message.contains(fmt::to_string(id)));
}

TEST_CASE("Copying a node the source index does not list fails as a missing file", "[octree][storage]") {
    const TempDirectory directory("octree_storage_copy_index_miss");
    const octree::IndexedMeshStorage source = octree::open_folder_indexed(directory.path() / "source");
    octree::MeshStorage target = octree::open_folder(directory.path() / "target");
    const octree::Id id = octree::Id::root();

    const auto result = target.copy_from(id, source);

    REQUIRE(!result.has_value());
    CHECK(result.error().holds(io::FileError(std::make_error_code(std::errc::no_such_file_or_directory), source.path_for(id))));
}

TEST_CASE("A failed node copy names the node", "[octree][storage]") {
    const TempDirectory directory("octree_storage_failed_copy");
    octree::MeshStorage target = octree::open_folder(directory.path() / "target");
    const octree::Id id = octree::Id::root();
    std::string message;

    SECTION("missing from the index") {
        const octree::IndexedMeshStorage source = octree::open_folder_indexed(directory.path() / "source");
        const auto result = target.copy_from(id, source);
        REQUIRE(!result.has_value());
        message = fmt::to_string(result.error());
    }

    SECTION("file missing from an unindexed storage") {
        const octree::MeshStorage source = octree::open_folder(directory.path() / "source");
        const auto result = target.copy_from(id, source);
        REQUIRE(!result.has_value());
        message = fmt::to_string(result.error());
    }

    CHECK(message.contains(fmt::to_string(id)));
}

TEST_CASE("A failed node copy names the file it could not replace", "[octree][storage]") {
    const TempDirectory directory("octree_storage_failed_copy_step");
    const octree::MeshStorage source = octree::open_folder(directory.path() / "source");
    octree::MeshStorage target = octree::open_folder(directory.path() / "target");
    target.settings().allow_overwrite = true;
    const octree::Id id = octree::Id::root();

    REQUIRE(io::write_bytes_to_path(std::vector<uint8_t>{1}, source.path_for(id)).has_value());
    std::filesystem::create_directories(target.path_for(id) / "child");

    const auto result = target.copy_from(id, source);

    REQUIRE(!result.has_value());
    const io::FileError *error = result.error().get_if<io::FileError>();
    REQUIRE(error != nullptr);
    CHECK(error->path() == target.path_for(id));
    // A link onto the directory fails as file_exists, so another code shows the removal reported its own failure.
    CHECK(error->code() != std::errc::file_exists);
}
