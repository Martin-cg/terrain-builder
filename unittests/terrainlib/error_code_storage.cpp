#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <string>
#include <string_view>

#include <fmt/format.h>

#include "error/CodeStorage.h"

namespace {

enum class FileError {
    NotFound,
};

std::string_view format_as(const FileError) {
    return "file not found";
}

// A code that carries data.
struct OffsetError {
    uint32_t offset;
};

std::string format_as(const OffsetError error) {
    return fmt::format("bad byte at {}", error.offset);
}

} // namespace

TEST_CASE("Code storage holds a code and its message", "[error][CodeStorage]") {
    const alp::CodeStorage<OffsetError> storage(OffsetError{.offset = 7});

    CHECK(storage.get().offset == 7);
    CHECK(storage.message() == "bad byte at 7");
}

TEST_CASE("Erased code storage keeps the message of the dropped code", "[error][CodeStorage]") {
    const alp::CodeStorage<FileError> typed(FileError::NotFound);
    const alp::CodeStorage<> erased(typed);

    CHECK(erased.message() == "file not found");
}

TEST_CASE("Erased code storage holds the message it was started with", "[error][CodeStorage]") {
    const alp::CodeStorage<> storage(std::string("round 3 made no progress"));

    CHECK(storage.message() == "round 3 made no progress");
}