#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <optional>
#include <source_location>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#include <fmt/format.h>
#include <tl/expected.hpp>

#include "error/Error.h"
#include "error/Result.h"
#include "error/format.h"
#include "error/utils.h"
#include "error_test_utils.h"

namespace {

enum class FileError {
    NotFound,
};

std::string_view format_as(const FileError) {
    return "file not found";
}

} // namespace

TEST_CASE("make_error creates an error with its code at the location it is handed", "[error][utils]") {
    const std::source_location location = std::source_location::current();
    const alp::Error<FileError> error = alp::make_error(FileError::NotFound, location);

    CHECK(error.code() == FileError::NotFound);
    REQUIRE(error.frames().size() == 1);
    CHECK(is_same_location(error.frames()[0].location, location));
}

TEST_CASE("make_error adds a message as the first context at the error's origin", "[error][utils]") {
    const alp::Error<FileError> error = alp::make_error(FileError::NotFound, "opening app.cfg");

    REQUIRE(error.frames().size() == 2);
    CHECK(error.frames()[1].message == "opening app.cfg");
    CHECK(is_same_location(error.frames()[1].location, error.frames()[0].location));
}

TEST_CASE("make_error creates an erased error from text at the location it is handed", "[error][utils]") {
    const std::source_location location = std::source_location::current();
    const alp::Error<> error = alp::make_error("round 3 made no progress", location);

    CHECK(fmt::to_string(error).contains("round 3 made no progress"));
    REQUIRE(error.frames().size() == 1);
    CHECK(is_same_location(error.frames()[0].location, location));
}

TEST_CASE("make_unexpected creates an error with its code", "[error][utils]") {
    const alp::Result<int, FileError> result = alp::make_unexpected(FileError::NotFound);

    REQUIRE(!result.has_value());
    CHECK(result.error().code() == FileError::NotFound);
    CHECK(result.error().frames().size() == 1);
}

TEST_CASE("make_unexpected adds a message as the first context", "[error][utils]") {
    const alp::Result<int, FileError> result = alp::make_unexpected(FileError::NotFound, "opening app.cfg");

    REQUIRE(!result.has_value());
    REQUIRE(result.error().frames().size() == 2);
    CHECK(result.error().frames()[1].message == "opening app.cfg");
}

TEST_CASE("make_unexpected records the location it is handed", "[error][utils]") {
    const std::source_location location = std::source_location::current();
    const alp::Result<int, FileError> result = alp::make_unexpected(FileError::NotFound, location);

    REQUIRE(!result.has_value());
    REQUIRE(result.error().frames().size() == 1);
    CHECK(is_same_location(result.error().frames()[0].location, location));
}

TEST_CASE("make_unexpected creates an erased error from a message at the location it is handed", "[error][utils]") {
    const std::source_location location = std::source_location::current();
    const alp::Result<int> result = alp::make_unexpected("round 3 made no progress", location);

    REQUIRE(!result.has_value());
    CHECK(fmt::to_string(result.error()).contains("round 3 made no progress"));
    REQUIRE(result.error().frames().size() == 1);
    CHECK(is_same_location(result.error().frames()[0].location, location));
}

TEST_CASE("propagate passes an error on as the same type", "[error][utils]") {
    STATIC_REQUIRE(std::is_same_v<decltype(alp::propagate(std::declval<alp::Error<FileError>>())), tl::unexpected<alp::Error<FileError>>>);
}

TEST_CASE("propagate adds a frame without a message at the location it is handed", "[error][utils]") {
    const std::source_location location = std::source_location::current();
    const alp::Result<int, FileError> result = alp::propagate(alp::make_error(FileError::NotFound), location);

    REQUIRE(!result.has_value());
    REQUIRE(result.error().frames().size() == 2);
    CHECK(result.error().frames()[1].message == std::nullopt);
    CHECK(is_same_location(result.error().frames()[1].location, location));
}

TEST_CASE("propagate passes a const error on and leaves it unchanged", "[error][utils]") {
    const auto error = alp::make_error(FileError::NotFound);
    const alp::Result<int, FileError> result = alp::propagate(error);

    REQUIRE(!result.has_value());
    CHECK(result.error().frames().size() == 2);
    CHECK(error.frames().size() == 1);
}

TEST_CASE("to_expected passes a value through", "[error][utils]") {
    const alp::Result<int, FileError> result = alp::to_expected(std::optional<int>(3), FileError::NotFound);

    CHECK(result.value() == 3);
}

TEST_CASE("to_expected turns an empty optional into an error at the location it is handed", "[error][utils]") {
    const std::source_location location = std::source_location::current();
    const alp::Result<int, FileError> result = alp::to_expected(std::optional<int>(), FileError::NotFound, location);

    REQUIRE(!result.has_value());
    CHECK(result.error().code() == FileError::NotFound);
    REQUIRE(result.error().frames().size() == 1);
    CHECK(is_same_location(result.error().frames()[0].location, location));
}

TEST_CASE("to_expected adds a message as the first context", "[error][utils]") {
    const alp::Result<int, FileError> result = alp::to_expected(std::optional<int>(), FileError::NotFound, "looking up the mesh");

    REQUIRE(!result.has_value());
    REQUIRE(result.error().frames().size() == 2);
    CHECK(result.error().frames()[1].message == "looking up the mesh");
}

TEST_CASE("to_expected turns an empty optional into an erased error from text", "[error][utils]") {
    const alp::Result<int> result = alp::to_expected(std::optional<int>(), "the mesh is missing");

    REQUIRE(!result.has_value());
    CHECK(fmt::to_string(result.error()).contains("the mesh is missing"));
}

TEST_CASE("context adds a frame at the location it is handed to a failed result", "[error][utils]") {
    alp::Result<int, FileError> failed = alp::make_unexpected(FileError::NotFound);
    const std::source_location location = std::source_location::current();
    const alp::Result<int, FileError> result = alp::context(std::move(failed), "loading the configuration", location);

    REQUIRE(!result.has_value());
    REQUIRE(result.error().frames().size() == 2);
    CHECK(result.error().frames()[1].message == "loading the configuration");
    CHECK(is_same_location(result.error().frames()[1].location, location));
}

TEST_CASE("context leaves a succeeded result alone", "[error][utils]") {
    const alp::Result<int, FileError> result = alp::context(alp::Result<int, FileError>(3), "loading the configuration");

    CHECK(result.value() == 3);
}

TEST_CASE("with_context builds its message only for a failed result", "[error][utils]") {
    uint32_t calls = 0;
    const auto make_message = [&calls]() {
        calls++;
        return std::string("loading the configuration");
    };

    const alp::Result<int, FileError> succeeded = alp::with_context(alp::Result<int, FileError>(3), make_message);
    CHECK(succeeded.value() == 3);
    CHECK(calls == 0);

    const alp::Result<int, FileError> failed = alp::with_context(alp::Result<int, FileError>(alp::make_unexpected(FileError::NotFound)), make_message);
    CHECK(calls == 1);
    REQUIRE(!failed.has_value());
    REQUIRE(failed.error().frames().size() == 2);
    CHECK(failed.error().frames()[1].message == "loading the configuration");
}