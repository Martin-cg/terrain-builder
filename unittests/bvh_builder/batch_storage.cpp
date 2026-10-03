#include "../catch2_helpers.h"
#include "../temp_directory.h"

#include <filesystem>
#include <utility>

#include "frontier/Storage.h"
#include "frontier/layout.h"

TEST_CASE("BatchStorage::remove deletes its own directory and nothing else", "[storage][BatchStorage]") {
    const TempDirectory directory("batch_storage");

    // A sibling under the same parent, so removing the parent instead of the storage would be caught.
    const frontier::Batch kept_batch{.entries = {{.morton = 7, .header = {}, .ref = {.batch = 0, .node = 0}}}};
    frontier::Storage kept(directory.path() / "kept");
    REQUIRE(kept.write(kept_batch).has_value());
    REQUIRE(kept.save_index().has_value());

    frontier::Storage removed(directory.path() / "removed");
    REQUIRE(removed.write(frontier::Batch{.entries = {{.morton = 3, .header = {}, .ref = {.batch = 0, .node = 1}}}}).has_value());
    REQUIRE(removed.write(frontier::Batch{}).has_value());
    REQUIRE(removed.save_index().has_value());

    REQUIRE(std::move(removed).remove().has_value());

    CHECK_FALSE(std::filesystem::exists(directory.path() / "removed"));
    const auto kept_result = kept.read(0);
    REQUIRE(kept_result.has_value());
    CHECK(kept_result.value() == kept_batch);
}

TEST_CASE("BatchStorage::remove leaves nothing for the dropped storage to write back", "[storage][BatchStorage]") {
    const TempDirectory directory("batch_storage");

    {
        // The index is left unsaved, so a storage that still thought itself dirty would write it on destruction.
        frontier::Storage storage(directory.path() / "removed");
        REQUIRE(storage.write(frontier::Batch{}).has_value());

        REQUIRE(std::move(storage).remove().has_value());
    }

    CHECK_FALSE(std::filesystem::exists(directory.path() / "removed"));
}

TEST_CASE("BatchStorage::remove accepts a storage that never wrote anything", "[storage][BatchStorage]") {
    const TempDirectory directory("batch_storage");
    frontier::Storage storage(directory.path() / "never_written");

    REQUIRE(std::move(storage).remove().has_value());

    CHECK_FALSE(std::filesystem::exists(directory.path() / "never_written"));
}
