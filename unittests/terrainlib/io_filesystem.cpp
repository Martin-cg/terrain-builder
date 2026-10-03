#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <filesystem>
#include <set>
#include <vector>

#include "../result_helpers.h"
#include "../temp_directory.h"
#include "error/prelude.h"
#include "io/FileError.h"
#include "io/bytes.h"
#include "io/filesystem.h"

namespace {

// Writes a one-byte file at `path`.
void write_file(const std::filesystem::path &path) {
    REQUIRE_VALUE(io::write_bytes_to_path(std::vector<uint8_t>{1}, path));
}

} // namespace

TEST_CASE("A directory is reported as an existing directory only", "[io][filesystem]") {
    const TempDirectory directory("io_filesystem_directory");

    CHECK(REQUIRE_VALUE(io::filesystem::exists(directory.path())));
    CHECK(REQUIRE_VALUE(io::filesystem::is_directory(directory.path())));
    CHECK(!REQUIRE_VALUE(io::filesystem::is_regular_file(directory.path())));
    CHECK(!REQUIRE_VALUE(io::filesystem::is_symlink(directory.path())));
}

TEST_CASE("A regular file is reported as an existing regular file only", "[io][filesystem]") {
    const TempDirectory directory("io_filesystem_regular_file");
    const std::filesystem::path file = directory.path() / "file";
    write_file(file);

    CHECK(REQUIRE_VALUE(io::filesystem::exists(file)));
    CHECK(!REQUIRE_VALUE(io::filesystem::is_directory(file)));
    CHECK(REQUIRE_VALUE(io::filesystem::is_regular_file(file)));
    CHECK(!REQUIRE_VALUE(io::filesystem::is_symlink(file)));
}

TEST_CASE("A symlink to a regular file is reported as a symlink and, followed, as a regular file", "[io][filesystem]") {
    const TempDirectory directory("io_filesystem_symlink");
    const std::filesystem::path file = directory.path() / "file";
    const std::filesystem::path link = directory.path() / "link";
    write_file(file);
    std::filesystem::create_symlink(file.filename(), link);

    CHECK(REQUIRE_VALUE(io::filesystem::exists(link)));
    CHECK(!REQUIRE_VALUE(io::filesystem::is_directory(link)));
    CHECK(REQUIRE_VALUE(io::filesystem::is_regular_file(link)));
    CHECK(REQUIRE_VALUE(io::filesystem::is_symlink(link)));
}

TEST_CASE("A broken symlink is reported as a symlink to nothing", "[io][filesystem]") {
    const TempDirectory directory("io_filesystem_broken_symlink");
    const std::filesystem::path link = directory.path() / "link";
    std::filesystem::create_symlink("missing", link);

    CHECK(!REQUIRE_VALUE(io::filesystem::exists(link)));
    CHECK(!REQUIRE_VALUE(io::filesystem::is_directory(link)));
    CHECK(!REQUIRE_VALUE(io::filesystem::is_regular_file(link)));
    CHECK(REQUIRE_VALUE(io::filesystem::is_symlink(link)));
}

TEST_CASE("A missing path is reported as nothing", "[io][filesystem]") {
    const TempDirectory directory("io_filesystem_missing");
    const std::filesystem::path missing = directory.path() / "missing";

    CHECK(!REQUIRE_VALUE(io::filesystem::exists(missing)));
    CHECK(!REQUIRE_VALUE(io::filesystem::is_directory(missing)));
    CHECK(!REQUIRE_VALUE(io::filesystem::is_regular_file(missing)));
    CHECK(!REQUIRE_VALUE(io::filesystem::is_symlink(missing)));
}

TEST_CASE("A directory is reported as empty until it holds a file", "[io][filesystem]") {
    const TempDirectory directory("io_filesystem_empty");

    CHECK(REQUIRE_VALUE(io::filesystem::is_empty(directory.path())));
    CHECK(REQUIRE_VALUE(io::filesystem::is_empty_directory(directory.path())));
    CHECK(REQUIRE_VALUE(io::filesystem::is_missing_or_empty_directory(directory.path())));

    write_file(directory.path() / "file");

    CHECK(!REQUIRE_VALUE(io::filesystem::is_empty(directory.path())));
    CHECK(!REQUIRE_VALUE(io::filesystem::is_empty_directory(directory.path())));
    CHECK(!REQUIRE_VALUE(io::filesystem::is_missing_or_empty_directory(directory.path())));
}

TEST_CASE("A regular file is not reported as an empty directory", "[io][filesystem]") {
    const TempDirectory directory("io_filesystem_empty_file");
    const std::filesystem::path file = directory.path() / "file";
    write_file(file);

    CHECK(!REQUIRE_VALUE(io::filesystem::is_empty_directory(file)));
    CHECK(!REQUIRE_VALUE(io::filesystem::is_missing_or_empty_directory(file)));
}

TEST_CASE("A missing path is reported as missing, not as an empty directory", "[io][filesystem]") {
    const TempDirectory directory("io_filesystem_empty_missing");
    const std::filesystem::path missing = directory.path() / "missing";

    CHECK(!REQUIRE_VALUE(io::filesystem::is_empty_directory(missing)));
    CHECK(REQUIRE_VALUE(io::filesystem::is_missing_or_empty_directory(missing)));

    const alp::Error<io::FileError> error = REQUIRE_ERROR(io::filesystem::is_empty(missing));
    CHECK(error.holds(io::make_file_not_found(missing)));
}

TEST_CASE("Removing a path removes everything below it, and nothing if it is missing", "[io][filesystem]") {
    const TempDirectory directory("io_filesystem_remove_all");
    const std::filesystem::path tree = directory.path() / "tree";
    write_file(tree / "a");
    write_file(tree / "nested" / "b");

    REQUIRE_VALUE(io::filesystem::remove_all(tree));
    CHECK(!REQUIRE_VALUE(io::filesystem::exists(tree)));

    REQUIRE_VALUE(io::filesystem::remove_all(tree));
}

TEST_CASE("Visiting files recursively visits every file in nested directories and no directory", "[io][filesystem]") {
    const TempDirectory directory("io_filesystem_visit_all");
    const std::set<std::filesystem::path> files = {
        directory.path() / "a",
        directory.path() / "nested" / "b",
        directory.path() / "nested" / "deeper" / "c",
    };
    for (const std::filesystem::path &file : files) {
        write_file(file);
    }

    std::set<std::filesystem::path> visited;
    REQUIRE_VALUE(io::filesystem::visit_files_recursively(directory.path(), [&](const std::filesystem::path &path) {
        visited.insert(path);
        return true;
    }));

    CHECK(visited == files);
}

TEST_CASE("Visiting files recursively stops once the visitor returns false", "[io][filesystem]") {
    const TempDirectory directory("io_filesystem_visit_stop");
    write_file(directory.path() / "a");
    write_file(directory.path() / "b");
    write_file(directory.path() / "c");

    uint32_t visit_count = 0;
    REQUIRE_VALUE(io::filesystem::visit_files_recursively(directory.path(), [&](const std::filesystem::path &) {
        visit_count++;
        return false;
    }));

    CHECK(visit_count == 1);
}

TEST_CASE("Visiting files below a missing path fails, naming the path", "[io][filesystem]") {
    const TempDirectory directory("io_filesystem_visit_missing");
    const std::filesystem::path missing = directory.path() / "missing";

    const alp::Error<io::FileError> error = REQUIRE_ERROR(io::filesystem::visit_files_recursively(missing, [](const std::filesystem::path &) {
        return true;
    }));

    CHECK(error.holds(io::make_file_not_found(missing)));
}
