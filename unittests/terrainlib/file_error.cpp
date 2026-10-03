#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <system_error>

#include "io/FileError.h"

TEST_CASE("A file error compares equal to the condition its code matches") {
    const io::FileError error(std::make_error_code(std::errc::no_such_file_or_directory), "some/file");

    CHECK(error == std::errc::no_such_file_or_directory);
    CHECK(error != std::errc::permission_denied);
}
