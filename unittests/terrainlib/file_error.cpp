#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <string>
#include <system_error>

#include <fmt/format.h>

#include "io/FileError.h"

TEST_CASE("A file error compares equal to the condition its code matches") {
    const io::FileError error(std::make_error_code(std::errc::no_such_file_or_directory), "some/file");

    CHECK(error == std::errc::no_such_file_or_directory);
    CHECK(error != std::errc::permission_denied);
}

TEST_CASE("A file error formats its path and the message of its code") {
    const std::error_code code = std::make_error_code(std::errc::permission_denied);
    const std::string text = fmt::to_string(io::FileError(code, "some/file"));

    CHECK(text.contains("some/file"));
    CHECK(text.contains(code.message()));
}
