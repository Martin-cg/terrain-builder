#include <catch2/catch_test_macros.hpp>

#include <string>
#include <string_view>

#include <fmt/format.h>

#include "error/Error.h"
#include "error/Trace.h"
#include "error/format.h"

namespace {

enum class FileError {
    NotFound,
};

std::string_view format_as(const FileError) {
    return "file not found";
}

enum class ParseError {
    BadDigit,
};

std::string_view format_as(const ParseError) {
    return "bad digit";
}

} // namespace

TEST_CASE("An error's one-line form mentions every context and its code's message", "[error][format]") {
    alp::Error<FileError> error(FileError::NotFound, "opening app.cfg");
    error.context("reading the configuration");
    const std::string text = fmt::to_string(error);

    CHECK(text.contains("opening app.cfg"));
    CHECK(text.contains("reading the configuration"));
    CHECK(text.contains("file not found"));
}

TEST_CASE("An error in a set mentions the code it holds", "[error][format]") {
    const alp::Error<FileError, ParseError> error(ParseError::BadDigit);
    const std::string text = fmt::to_string(error);

    CHECK(text.contains("bad digit"));
    CHECK(!text.contains("file not found"));
}

TEST_CASE("An error's report mentions every message and the file and line of every frame", "[error][format]") {
    alp::Error<FileError> error(FileError::NotFound);
    error.context("reading the configuration");
    const std::string report = fmt::format("{:#}", error);

    CHECK(report.contains("reading the configuration"));
    CHECK(report.contains("file not found"));
    for (const alp::Frame &frame : error.frames()) {
        CHECK(report.contains(frame.location.file_name()));
        CHECK(report.contains(std::to_string(frame.location.line())));
    }
}