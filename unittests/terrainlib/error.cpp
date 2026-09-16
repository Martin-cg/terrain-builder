#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <memory>
#include <optional>
#include <source_location>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include <fmt/format.h>
#include <tl/expected.hpp>

#include "error/CodeStorage.h"
#include "error/Error.h"
#include "error/ErrorCode.h"
#include "error/Result.h"
#include "error/Trace.h"
#include "error/format.h"
#include "error/try.h"
#include "error/utils.h"

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

// A code that carries data.
struct OffsetError {
    uint32_t offset;

    bool operator==(const OffsetError &) const = default;
};

std::string format_as(const OffsetError error) {
    return fmt::format("bad byte at {}", error.offset);
}

// A code that also converts to a string view.
struct NamedError {
    std::string_view name;

    operator std::string_view() const {
        return this->name;
    }

    bool operator==(const NamedError &) const = default;
};

std::string_view format_as(const NamedError error) {
    return error.name;
}

enum class Unformatted {
    Broken,
};

// A formattable class without equality.
struct Incomparable {};

std::string_view format_as(const Incomparable) {
    return "incomparable";
}

// Whether `frame` was recorded on the line after `before`, in the same file.
bool is_recorded_after(const alp::Frame &frame, const std::source_location before) {
    const bool is_same_file = std::string_view(frame.location.file_name()) == before.file_name();
    return is_same_file && frame.location.line() == before.line() + 1;
}

// Whether Es form an error.
template <typename... Es>
concept FormsError = requires { typename alp::Error<Es...>; };

// Returns `result` from a function whose error is erased.
alp::Result<int> erase_result(alp::Result<int, FileError> result) {
    return result;
}

} // namespace

TEST_CASE("Error codes are unqualified formattable enums and classes other than strings") {
    STATIC_REQUIRE(alp::ErrorCode<FileError>);
    STATIC_REQUIRE(alp::ErrorCode<OffsetError>);
    STATIC_REQUIRE(alp::ErrorCode<NamedError>);
    STATIC_REQUIRE(!alp::ErrorCode<Unformatted>);
    STATIC_REQUIRE(!alp::ErrorCode<Incomparable>);
    STATIC_REQUIRE(!alp::ErrorCode<int>);
    STATIC_REQUIRE(!alp::ErrorCode<const char *>);
    STATIC_REQUIRE(!alp::ErrorCode<std::string>);
    STATIC_REQUIRE(!alp::ErrorCode<std::string_view>);
    STATIC_REQUIRE(!alp::ErrorCode<const FileError>);
    STATIC_REQUIRE(!alp::ErrorCode<FileError &>);
}

TEST_CASE("An error without codes is erased") {
    STATIC_REQUIRE(!alp::Error<FileError>::is_erased);
    STATIC_REQUIRE(alp::Error<>::is_erased);
}

TEST_CASE("An error is created explicitly from one of its codes, an erased error from a message") {
    STATIC_REQUIRE(std::is_constructible_v<alp::Error<FileError>, FileError>);
    STATIC_REQUIRE(!std::is_convertible_v<FileError, alp::Error<FileError>>);
    STATIC_REQUIRE(!std::is_constructible_v<alp::Error<FileError>, ParseError>);
    STATIC_REQUIRE(!std::is_constructible_v<alp::Error<FileError>, std::string>);
    STATIC_REQUIRE(std::is_constructible_v<alp::Error<>, std::string>);
    STATIC_REQUIRE(!std::is_convertible_v<std::string, alp::Error<>>);
    STATIC_REQUIRE(!std::is_constructible_v<alp::Error<>, FileError>);
}

TEST_CASE("A typed error converts into an erased error but not into another typed error") {
    STATIC_REQUIRE(std::is_convertible_v<alp::Error<FileError>, alp::Error<>>);
    STATIC_REQUIRE(!std::is_constructible_v<alp::Error<FileError>, alp::Error<>>);
    STATIC_REQUIRE(!std::is_constructible_v<alp::Error<FileError>, alp::Error<ParseError>>);
}

TEST_CASE("A result converts from a result whose error converts, but not from a plain code") {
    STATIC_REQUIRE(std::is_convertible_v<alp::Result<int, FileError>, alp::Result<int>>);
    STATIC_REQUIRE(!std::is_convertible_v<alp::Result<int>, alp::Result<int, FileError>>);
    STATIC_REQUIRE(!std::is_convertible_v<tl::unexpected<FileError>, alp::Result<int, FileError>>);
    STATIC_REQUIRE(!std::is_convertible_v<tl::expected<int, FileError>, alp::Result<int, FileError>>);
}

TEST_CASE("propagate passes an error on as the same type") {
    STATIC_REQUIRE(std::is_same_v<decltype(alp::propagate(std::declval<alp::Error<FileError>>())), tl::unexpected<alp::Error<FileError>>>);
}

TEST_CASE("An error set is created from any of its codes and converts into any set that includes them") {
    STATIC_REQUIRE(std::is_constructible_v<alp::Error<FileError, ParseError>, ParseError>);
    STATIC_REQUIRE(!std::is_constructible_v<alp::Error<FileError, ParseError>, OffsetError>);
    STATIC_REQUIRE(std::is_convertible_v<alp::Error<FileError>, alp::Error<FileError, ParseError>>);
    STATIC_REQUIRE(std::is_convertible_v<const alp::Error<FileError> &, alp::Error<FileError, ParseError>>);
    STATIC_REQUIRE(std::is_convertible_v<alp::Error<FileError, ParseError>, alp::Error<ParseError, FileError>>);
    STATIC_REQUIRE(std::is_convertible_v<alp::Error<FileError, ParseError>, alp::Error<FileError, ParseError, OffsetError>>);
    STATIC_REQUIRE(std::is_convertible_v<alp::Error<FileError, ParseError>, alp::Error<>>);
    STATIC_REQUIRE(!std::is_constructible_v<alp::Error<FileError>, alp::Error<FileError, ParseError>>);
    STATIC_REQUIRE(!std::is_constructible_v<alp::Error<FileError, ParseError>, alp::Error<OffsetError>>);
    STATIC_REQUIRE(!std::is_constructible_v<alp::Error<FileError, ParseError>, alp::Error<>>);
    STATIC_REQUIRE(std::is_convertible_v<alp::Result<int, FileError>, alp::Result<int, FileError, ParseError>>);
}

TEST_CASE("A nested error set expands into its codes, each kept once in the order it first appears") {
    STATIC_REQUIRE(std::is_same_v<alp::Error<FileError, ParseError>, alp::BasicError<FileError, ParseError>>);
    STATIC_REQUIRE(std::is_same_v<alp::Error<alp::Error<FileError, ParseError>, OffsetError>, alp::BasicError<FileError, ParseError, OffsetError>>);
    STATIC_REQUIRE(std::is_same_v<alp::Error<FileError, alp::Error<ParseError, FileError>>, alp::BasicError<FileError, ParseError>>);
    STATIC_REQUIRE(std::is_same_v<alp::Error<alp::Error<FileError, ParseError>>, alp::BasicError<FileError, ParseError>>);
    STATIC_REQUIRE(std::is_same_v<alp::Result<int, alp::Error<FileError>, ParseError>, tl::expected<int, alp::BasicError<FileError, ParseError>>>);
}

TEST_CASE("An error set is not a code, and an erased set cannot be nested") {
    STATIC_REQUIRE(!alp::ErrorCode<alp::BasicError<FileError>>);
    STATIC_REQUIRE(!FormsError<alp::Error<>, FileError>);
}

TEST_CASE("An error set includes the codes of a nested set") {
    STATIC_REQUIRE(alp::BasicError<FileError, ParseError, OffsetError>::includes<alp::Error<FileError, ParseError>>);
    STATIC_REQUIRE(!alp::BasicError<FileError>::includes<alp::Error<FileError, ParseError>>);
}

TEST_CASE("A trace starts where the error originated and grows innermost first") {
    alp::Trace trace(std::source_location::current());
    trace.push(alp::Frame{.message = "outer", .location = std::source_location::current()});

    REQUIRE(trace.frames().size() == 2);
    CHECK(!trace.frames()[0].message.has_value());
    CHECK(trace.frames()[1].message == "outer");
}

TEST_CASE("Code storage holds a code and its message") {
    const alp::CodeStorage<OffsetError> storage(OffsetError{.offset = 7});

    CHECK(storage.get().offset == 7);
    CHECK(storage.message() == "bad byte at 7");
}

TEST_CASE("Erased code storage keeps the message of the dropped code") {
    const alp::CodeStorage<FileError> typed(FileError::NotFound);
    const alp::CodeStorage<> erased(typed);

    CHECK(erased.message() == "file not found");
}

TEST_CASE("Erased code storage holds the message it was started with") {
    const alp::CodeStorage<> storage(std::string("round 3 made no progress"));

    CHECK(storage.message() == "round 3 made no progress");
}

TEST_CASE("An error records its code and where it originated") {
    const std::source_location before = std::source_location::current();
    const alp::Error<FileError> error(FileError::NotFound);

    CHECK(error.code() == FileError::NotFound);
    REQUIRE(error.frames().size() == 1);
    CHECK(!error.frames()[0].message.has_value());
    CHECK(is_recorded_after(error.frames()[0], before));
}

TEST_CASE("An error created with a message has it as its first context") {
    const alp::Error<FileError> error(FileError::NotFound, "opening app.cfg");

    REQUIRE(error.frames().size() == 2);
    CHECK(!error.frames()[0].message.has_value());
    CHECK(error.frames()[1].message == "opening app.cfg");
    CHECK(error.frames()[1].location.line() == error.frames()[0].location.line());
}

TEST_CASE("An erased error starts from a message") {
    const std::source_location before = std::source_location::current();
    const alp::Error<> error("round 3 made no progress");

    CHECK(fmt::format("{}", error) == "round 3 made no progress");
    REQUIRE(error.frames().size() == 1);
    CHECK(!error.frames()[0].message.has_value());
    CHECK(is_recorded_after(error.frames()[0], before));
}

TEST_CASE("Context adds a frame where it is called") {
    alp::Error<FileError> error(FileError::NotFound);
    const std::source_location before = std::source_location::current();
    error.context("reading the configuration");

    REQUIRE(error.frames().size() == 2);
    CHECK(error.frames()[1].message == "reading the configuration");
    CHECK(is_recorded_after(error.frames()[1], before));
}

TEST_CASE("Context chains on a temporary error") {
    const alp::Error<FileError> error = alp::Error<FileError>(FileError::NotFound).context("inner").context("outer");

    REQUIRE(error.frames().size() == 3);
    CHECK(error.frames()[1].message == "inner");
    CHECK(error.frames()[2].message == "outer");
}

TEST_CASE("Erasure keeps the frames and the code's message") {
    alp::Error<FileError> typed(FileError::NotFound, "opening app.cfg");
    typed.context("reading the configuration");
    const alp::Error<> erased = std::move(typed);

    REQUIRE(erased.frames().size() == 3);
    CHECK(erased.frames()[2].message == "reading the configuration");
    CHECK(fmt::format("{}", erased) == "reading the configuration: opening app.cfg: file not found");
}

TEST_CASE("make_unexpected records its caller") {
    const std::source_location before = std::source_location::current();
    const alp::Result<int, FileError> result = alp::make_unexpected(FileError::NotFound);

    REQUIRE(!result.has_value());
    CHECK(result.error().code() == FileError::NotFound);
    REQUIRE(result.error().frames().size() == 1);
    CHECK(is_recorded_after(result.error().frames()[0], before));
}

TEST_CASE("make_unexpected adds a message as the first context") {
    const alp::Result<int, FileError> result = alp::make_unexpected(FileError::NotFound, "opening app.cfg");

    REQUIRE(!result.has_value());
    REQUIRE(result.error().frames().size() == 2);
    CHECK(result.error().frames()[1].message == "opening app.cfg");
}

TEST_CASE("make_unexpected records a location it is handed") {
    const std::source_location elsewhere = std::source_location::current();
    const alp::Result<int, FileError> result = alp::make_unexpected(FileError::NotFound, std::nullopt, elsewhere);

    REQUIRE(!result.has_value());
    REQUIRE(result.error().frames().size() == 1);
    CHECK(result.error().frames()[0].location.line() == elsewhere.line());
}

TEST_CASE("make_unexpected starts an erased error from a message") {
    const std::source_location before = std::source_location::current();
    const alp::Result<int> result = alp::make_unexpected("round 3 made no progress");

    REQUIRE(!result.has_value());
    CHECK(fmt::format("{}", result.error()) == "round 3 made no progress");
    REQUIRE(result.error().frames().size() == 1);
    CHECK(is_recorded_after(result.error().frames()[0], before));
}

TEST_CASE("propagate passes an error on with a frame at its caller") {
    alp::Error<FileError> error(FileError::NotFound, "opening app.cfg");
    const std::source_location before = std::source_location::current();
    const alp::Result<int, FileError> result = alp::propagate(std::move(error));

    REQUIRE(!result.has_value());
    CHECK(result.error().code() == FileError::NotFound);
    REQUIRE(result.error().frames().size() == 3);
    CHECK(result.error().frames()[1].message == "opening app.cfg");
    CHECK(!result.error().frames()[2].message.has_value());
    CHECK(is_recorded_after(result.error().frames()[2], before));
}

TEST_CASE("propagate passes a const error on and leaves it unchanged") {
    const alp::Error<FileError> error(FileError::NotFound);
    const alp::Result<int, FileError> result = alp::propagate(error);

    REQUIRE(!result.has_value());
    CHECK(result.error().frames().size() == 2);
    CHECK(error.frames().size() == 1);
}

TEST_CASE("A typed error returns from a function whose error is erased") {
    const alp::Result<int> result = alp::make_unexpected(FileError::NotFound);

    REQUIRE(!result.has_value());
    CHECK(fmt::format("{}", result.error()) == "file not found");
}

TEST_CASE("A typed result converts to an erased result") {
    CHECK(erase_result(3).value() == 3);

    const alp::Result<int> result = erase_result(alp::make_unexpected(FileError::NotFound, "opening app.cfg"));
    REQUIRE(!result.has_value());
    CHECK(fmt::format("{}", result.error()) == "opening app.cfg: file not found");
}

TEST_CASE("A result without a value carries an error") {
    const alp::Result<void, FileError> result = alp::make_unexpected(FileError::NotFound);

    REQUIRE(!result.has_value());
    CHECK(result.error().code() == FileError::NotFound);
}

TEST_CASE("to_expected passes a value through") {
    const alp::Result<int, FileError> result = alp::to_expected(std::optional<int>(3), FileError::NotFound);

    CHECK(result.value() == 3);
}

TEST_CASE("to_expected turns an empty optional into an error at its caller") {
    const std::source_location before = std::source_location::current();
    const alp::Result<int, FileError> result = alp::to_expected(std::optional<int>(), FileError::NotFound);

    REQUIRE(!result.has_value());
    CHECK(result.error().code() == FileError::NotFound);
    REQUIRE(result.error().frames().size() == 1);
    CHECK(is_recorded_after(result.error().frames()[0], before));
}

TEST_CASE("to_expected adds a message as the first context") {
    const alp::Result<int, FileError> result = alp::to_expected(std::optional<int>(), FileError::NotFound, "looking up the mesh");

    REQUIRE(!result.has_value());
    REQUIRE(result.error().frames().size() == 2);
    CHECK(result.error().frames()[1].message == "looking up the mesh");
}

TEST_CASE("context adds a frame to a failed result") {
    alp::Result<int, FileError> failed = alp::make_unexpected(FileError::NotFound);
    const std::source_location before = std::source_location::current();
    const alp::Result<int, FileError> result = alp::context(std::move(failed), "loading the configuration");

    REQUIRE(!result.has_value());
    REQUIRE(result.error().frames().size() == 2);
    CHECK(result.error().frames()[1].message == "loading the configuration");
    CHECK(is_recorded_after(result.error().frames()[1], before));
}

TEST_CASE("context leaves a succeeded result alone") {
    const alp::Result<int, FileError> result = alp::context(alp::Result<int, FileError>(3), "loading the configuration");

    CHECK(result.value() == 3);
}

TEST_CASE("with_context builds its message only for a failed result") {
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

TEST_CASE("An error formats on one line, outermost message first") {
    alp::Error<FileError> error(FileError::NotFound, "opening app.cfg");
    error.context("reading the configuration");
    error.context("loading the application");

    CHECK(fmt::format("{}", error) == "loading the application: reading the configuration: opening app.cfg: file not found");
}

TEST_CASE("An error without a message formats its code's message alone") {
    alp::Error<FileError> error(FileError::NotFound);
    error.context("reading the configuration");

    CHECK(fmt::format("{}", error) == "reading the configuration: file not found");
}

TEST_CASE("An error formats one frame per line, newest first, with the line of each") {
    alp::Error<FileError> error(FileError::NotFound);
    error.context("reading the configuration");
    const std::string report = fmt::format("{:#}", error);

    const size_t context_position = report.find("reading the configuration");
    const size_t origin_position = report.find("file not found");
    REQUIRE(context_position != std::string::npos);
    REQUIRE(origin_position != std::string::npos);
    CHECK(context_position < origin_position);

    for (const alp::Frame &frame : error.frames()) {
        CHECK(report.contains(fmt::format(":{}", frame.location.line())));
    }
}

TEST_CASE("An error formats its frames after its one-line form") {
    alp::Error<FileError> error(FileError::NotFound);
    error.context("reading the configuration");
    const std::string report = fmt::format("{:#}", error);

    CHECK(report.starts_with("reading the configuration: file not found\n"));
}

TEST_CASE("An error names the file relative to the source root, the line and the function of each frame") {
    const alp::Error<FileError> error(FileError::NotFound);
    const std::string report = fmt::format("{:#}", error);

    CHECK(report.contains(fmt::format("unittests/terrainlib/error.cpp:{}", error.frames()[0].location.line())));
    CHECK(report.contains(error.frames()[0].location.function_name()));
}

TEST_CASE("An error in a set formats the code it holds") {
    const alp::Error<FileError, ParseError> error(ParseError::BadDigit);

    CHECK(fmt::format("{}", error) == "bad digit");
    CHECK(error.frames().size() == 1);
}

TEST_CASE("An error widens into a set, keeping its frames") {
    alp::Error<FileError> error(FileError::NotFound, "opening app.cfg");
    const alp::Error<FileError, ParseError> widened = std::move(error);

    REQUIRE(widened.frames().size() == 2);
    CHECK(fmt::format("{}", widened) == "opening app.cfg: file not found");
}

TEST_CASE("A const error widens into a set and stays as it was") {
    const alp::Error<FileError> error(FileError::NotFound, "opening app.cfg");
    const alp::Error<FileError, ParseError> widened = error;

    CHECK(widened.frames().size() == 2);
    CHECK(error.frames().size() == 2);
    CHECK(fmt::format("{}", error) == "opening app.cfg: file not found");
}

TEST_CASE("A set erases, keeping its frames and the message of the code it holds") {
    alp::Error<FileError, ParseError> error(ParseError::BadDigit, "parsing the header");
    const alp::Error<> erased = std::move(error);

    REQUIRE(erased.frames().size() == 2);
    CHECK(fmt::format("{}", erased) == "parsing the header: bad digit");
}

TEST_CASE("make_unexpected returns a code into a set") {
    const alp::Result<int, FileError, ParseError> result = alp::make_unexpected(ParseError::BadDigit);

    REQUIRE(!result.has_value());
    CHECK(fmt::format("{}", result.error()) == "bad digit");
}

TEST_CASE("narrow gives the error with only the code it holds, keeping its frames") {
    alp::Error<FileError, ParseError> error(FileError::NotFound, "opening app.cfg");
    const tl::expected<alp::Error<FileError>, alp::Error<FileError, ParseError>> result = std::move(error).narrow<FileError>();

    REQUIRE(result.has_value());
    CHECK(result->code() == FileError::NotFound);
    REQUIRE(result->frames().size() == 2);
    CHECK(result->frames()[1].message == "opening app.cfg");
}

TEST_CASE("narrow hands the error back when it holds another code") {
    alp::Error<FileError, ParseError> error(ParseError::BadDigit, "parsing the header");
    const tl::expected<alp::Error<FileError>, alp::Error<FileError, ParseError>> result = std::move(error).narrow<FileError>();

    REQUIRE(!result.has_value());
    CHECK(result.error().frames().size() == 2);
    CHECK(fmt::format("{}", result.error()) == "parsing the header: bad digit");
}

TEST_CASE("narrow on a const error leaves it as it was") {
    const alp::Error<FileError, ParseError> error(FileError::NotFound, "opening app.cfg");
    const tl::expected<alp::Error<FileError>, alp::Error<FileError, ParseError>> result = error.narrow<FileError>();

    REQUIRE(result.has_value());
    CHECK(result->frames().size() == 2);
    CHECK(error.frames().size() == 2);
    CHECK(fmt::format("{}", error) == "opening app.cfg: file not found");
}

TEST_CASE("narrow to several codes keeps those it names") {
    using Narrowed = tl::expected<alp::Error<FileError, ParseError>, alp::Error<FileError, ParseError, OffsetError>>;

    const Narrowed matched = alp::Error<FileError, ParseError, OffsetError>(ParseError::BadDigit).narrow<FileError, ParseError>();
    REQUIRE(matched.has_value());
    CHECK(fmt::format("{}", *matched) == "bad digit");

    const Narrowed missed = alp::Error<FileError, ParseError, OffsetError>(OffsetError{.offset = 7}).narrow<FileError, ParseError>();
    REQUIRE(!missed.has_value());
    CHECK(fmt::format("{}", missed.error()) == "bad byte at 7");
}

TEST_CASE("narrow to a set gives the error with only that set's codes, keeping its frames") {
    alp::Error<alp::Error<FileError, ParseError>, OffsetError> error(ParseError::BadDigit, "parsing the header");
    const tl::expected<alp::Error<FileError, ParseError>, alp::Error<FileError, ParseError, OffsetError>> result = std::move(error).narrow<alp::Error<FileError, ParseError>>();

    REQUIRE(result.has_value());
    REQUIRE(result->frames().size() == 2);
    CHECK(fmt::format("{}", *result) == "parsing the header: bad digit");
}

TEST_CASE("narrow to a set hands the error back when it holds a code outside the set") {
    alp::Error<alp::Error<FileError, ParseError>, OffsetError> error(OffsetError{.offset = 7}, "reading the body");
    const tl::expected<alp::Error<FileError, ParseError>, alp::Error<FileError, ParseError, OffsetError>> result = std::move(error).narrow<alp::Error<FileError, ParseError>>();

    REQUIRE(!result.has_value());
    CHECK(result.error().frames().size() == 2);
    CHECK(fmt::format("{}", result.error()) == "reading the body: bad byte at 7");
}

TEST_CASE("holds is true for the held code and false for a different value or another code type") {
    const alp::Error<FileError, OffsetError> error(OffsetError{.offset = 7});

    CHECK(error.holds(OffsetError{.offset = 7}));
    CHECK(!error.holds(OffsetError{.offset = 8}));
    CHECK(!error.holds(FileError::NotFound));
}

TEST_CASE("Typed code storage compares by the code it holds, and erased storage does not compare") {
    const alp::CodeStorage<FileError, ParseError> storage(FileError::NotFound);

    CHECK(storage == alp::CodeStorage<FileError, ParseError>(FileError::NotFound));
    CHECK(storage != alp::CodeStorage<FileError, ParseError>(ParseError::BadDigit));
    STATIC_REQUIRE(!std::equality_comparable<alp::CodeStorage<>>);
}

TEST_CASE("get_if points at the held code if it has that type, and is null otherwise") {
    const alp::Error<FileError, OffsetError> error(OffsetError{.offset = 7});

    const OffsetError *offset_error = error.get_if<OffsetError>();
    REQUIRE(offset_error != nullptr);
    CHECK(offset_error->offset == 7);
    CHECK(error.get_if<FileError>() == nullptr);
}

TEST_CASE("An error of a nested set widens into the flattened set, keeping its frames") {
    alp::Error<FileError, ParseError> error(FileError::NotFound, "opening app.cfg");
    const alp::Error<alp::Error<FileError, ParseError>, OffsetError> widened = std::move(error);

    REQUIRE(widened.frames().size() == 2);
    CHECK(fmt::format("{}", widened) == "opening app.cfg: file not found");
}

// ALP_TRY and ALP_TRY_CONTEXT.

namespace {

// The message of a propagation, counting how often it was built.
std::string count_message(uint32_t &messages) {
    messages++;
    return "loading the configuration";
}

// Counts how it was handed around, so a copy on the way out of ALP_TRY shows up.
struct Tracked {
    uint32_t copies = 0;
    uint32_t moves = 0;

    Tracked() = default;
    Tracked(const Tracked &other) : copies(other.copies + 1), moves(other.moves) {}
    Tracked(Tracked &&other) noexcept : copies(other.copies), moves(other.moves + 1) {}
    Tracked &operator=(const Tracked &) = delete;
    Tracked &operator=(Tracked &&) = delete;
};

// A class built from the plain code, like LoadMeshError from LoadMeshErrorKind.
struct FileErrorWrapper {
    FileErrorWrapper(const FileError code) : code(code) {}

    FileError code;
};

struct Cancelled {};

// A value with a single member, for a structured binding with one name.
struct Single {
    int value;
};

// Holds a member that ALP_TRY assigns to.
struct Holder {
    int value = 0;

    alp::Result<void, FileError> load(alp::Result<int, FileError> source) {
        ALP_TRY(this->value, std::move(source));
        return {};
    }
};

alp::Result<int, FileError> forward_typed(alp::Result<int, FileError> source, std::source_location &before) {
    before = std::source_location::current();
    ALP_TRY(const int value, std::move(source));
    return value + 1;
}

alp::Result<int> forward_typed_to_erased(alp::Result<int, FileError> source, std::source_location &before) {
    before = std::source_location::current();
    ALP_TRY(const int value, std::move(source));
    return value + 1;
}

alp::Result<int> forward_erased(alp::Result<int> source) {
    ALP_TRY(const int value, std::move(source));
    return value + 1;
}

alp::Result<void, FileError> skip_typed(alp::Result<void, FileError> source, std::source_location &before) {
    before = std::source_location::current();
    ALP_TRY(std::move(source));
    return {};
}

alp::Result<int, FileError> forward_with_context(alp::Result<int, FileError> source, uint32_t &messages, std::source_location &before) {
    before = std::source_location::current();
    ALP_TRY_CONTEXT(const int value, std::move(source), count_message(messages));
    return value + 1;
}

alp::Result<void, FileError> skip_with_context(alp::Result<void, FileError> source, uint32_t &messages, std::source_location &before) {
    before = std::source_location::current();
    ALP_TRY_CONTEXT(std::move(source), count_message(messages));
    return {};
}

tl::expected<int, FileError> forward_code_to_code(tl::expected<int, FileError> source) {
    ALP_TRY(const int value, std::move(source));
    return value + 1;
}

tl::expected<int, FileErrorWrapper> forward_code_to_wrapper(tl::expected<int, FileError> source) {
    ALP_TRY(const int value, std::move(source));
    return value + 1;
}

alp::Result<int, FileError> forward_code_to_typed(tl::expected<int, FileError> source, std::source_location &before) {
    before = std::source_location::current();
    ALP_TRY(const int value, std::move(source));
    return value + 1;
}

alp::Result<int> forward_code_to_erased(tl::expected<int, FileError> source, std::source_location &before) {
    before = std::source_location::current();
    ALP_TRY(const int value, std::move(source));
    return value + 1;
}

alp::Result<int, FileError> forward_code_with_context(tl::expected<int, FileError> source, std::source_location &before) {
    before = std::source_location::current();
    ALP_TRY_CONTEXT(const int value, std::move(source), "reading app.cfg");
    return value + 1;
}

tl::expected<int, FileError> forward_code_with_context_to_code(tl::expected<int, FileError> source) {
    ALP_TRY_CONTEXT(const int value, std::move(source), "reading app.cfg");
    return value + 1;
}

tl::expected<int, std::string> forward_string(tl::expected<int, std::string> source) {
    ALP_TRY(const int value, std::move(source));
    return value + 1;
}

tl::expected<int, std::variant<FileError, Cancelled>> forward_variant(tl::expected<int, std::variant<FileError, Cancelled>> source) {
    ALP_TRY(const int value, std::move(source));
    return value + 1;
}

std::optional<int> forward_optional(std::optional<int> source) {
    ALP_TRY(const int value, std::move(source));
    return value + 1;
}

std::optional<int> skip_optional(std::optional<int> source) {
    ALP_TRY(std::move(source));
    return 0;
}

alp::Result<int, FileError> assign_to_existing(alp::Result<int, FileError> source) {
    int existing = 0;
    ALP_TRY(existing, std::move(source));
    return existing;
}

alp::Result<Tracked, FileError> forward_tracked(alp::Result<Tracked, FileError> source) {
    ALP_TRY(Tracked value, std::move(source));
    return value;
}

alp::Result<uint32_t, FileError> count_copies_behind_reference(alp::Result<Tracked, FileError> source) {
    ALP_TRY(const Tracked &value, std::move(source));
    return value.copies;
}

alp::Result<std::unique_ptr<int>, FileError> forward_move_only(alp::Result<std::unique_ptr<int>, FileError> source) {
    ALP_TRY(std::unique_ptr<int> pointer, std::move(source));
    return pointer;
}

alp::Result<int, FileError> unpack_single(alp::Result<Single, FileError> source) {
    ALP_TRY(const auto [value], std::move(source));
    return value;
}

// Hands `source` on, counting how often it was called.
alp::Result<int, FileError> count_call(uint32_t &calls, const alp::Result<int, FileError> &source) {
    calls++;
    return source;
}

alp::Result<int, FileError> forward_counted(uint32_t &calls, const alp::Result<int, FileError> &source) {
    ALP_TRY(const int value, count_call(calls, source));
    return value;
}

// Sums `sources` until one of them fails, counting how many it looked at.
alp::Result<int, FileError> sum_until_failure(const std::vector<alp::Result<int, FileError>> &sources, uint32_t &visited) {
    int sum = 0;
    for (const alp::Result<int, FileError> &source : sources) {
        visited++;
        ALP_TRY(const int value, source);
        sum += value;
    }
    return sum;
}

alp::Result<int, FileError> add_on_one_line(alp::Result<int, FileError> first, alp::Result<int, FileError> second) {
    ALP_TRY(const int left, std::move(first)); ALP_TRY(const int right, std::move(second));
    return left + right;
}

template <typename A, typename B>
alp::Result<std::pair<A, B>, FileError> make_pair_result(const A first, const B second) {
    return std::pair<A, B>{first, second};
}

alp::Result<int, FileError> add_pair() {
    ALP_TRY(const auto pair, (make_pair_result<int, int>(1, 2)));
    return pair.first + pair.second;
}

alp::Result<int, FileError> add_pair_with_context() {
    ALP_TRY_CONTEXT(const auto pair, (make_pair_result<int, int>(1, 2)), "adding the pair");
    return pair.first + pair.second;
}

// Uses the names of the macro's own parameters for its variables.
alp::Result<int, FileError> forward_with_parameter_names(alp::Result<int, FileError> result) {
    const std::string message = "app.cfg";
    ALP_TRY_CONTEXT(const int target, std::move(result), fmt::format("reading {}, then {}", message, "the index"));
    return target;
}

alp::Result<int, FileError> fail_inner() {
    return alp::make_unexpected(FileError::NotFound);
}

alp::Result<int, FileError> fail_middle() {
    ALP_TRY_CONTEXT(const int value, fail_inner(), "reading the configuration");
    return value;
}

alp::Result<int> fail_outer() {
    ALP_TRY(const int value, fail_middle());
    return value;
}

alp::Result<int, FileError, ParseError> forward_into_set(alp::Result<int, FileError> source, std::source_location &before) {
    before = std::source_location::current();
    ALP_TRY(const int value, std::move(source));
    return value + 1;
}

alp::Result<int, FileError, ParseError> forward_code_into_set(tl::expected<int, ParseError> source, std::source_location &before) {
    before = std::source_location::current();
    ALP_TRY(const int value, std::move(source));
    return value + 1;
}

} // namespace

TEST_CASE("ALP_TRY binds the value of a succeeded result") {
    std::source_location before;

    CHECK(forward_typed(3, before).value() == 4);
}

TEST_CASE("ALP_TRY propagates a failed result with a frame at its line") {
    std::source_location before;
    const alp::Result<int, FileError> result = forward_typed(alp::make_unexpected(FileError::NotFound), before);

    REQUIRE(!result.has_value());
    CHECK(result.error().code() == FileError::NotFound);
    REQUIRE(result.error().frames().size() == 2);
    CHECK(!result.error().frames()[1].message.has_value());
    CHECK(is_recorded_after(result.error().frames()[1], before));
}

TEST_CASE("ALP_TRY erases a typed error for an erased enclosing function") {
    std::source_location before;
    const alp::Result<int> result = forward_typed_to_erased(alp::make_unexpected(FileError::NotFound), before);

    REQUIRE(!result.has_value());
    CHECK(fmt::format("{}", result.error()) == "file not found");
    REQUIRE(result.error().frames().size() == 2);
    CHECK(is_recorded_after(result.error().frames()[1], before));
}

TEST_CASE("ALP_TRY passes an erased error on") {
    CHECK(forward_erased(3).value() == 4);

    const alp::Result<int> result = forward_erased(alp::make_unexpected("round 3 made no progress"));
    REQUIRE(!result.has_value());
    CHECK(fmt::format("{}", result.error()) == "round 3 made no progress");
    CHECK(result.error().frames().size() == 2);
}

TEST_CASE("ALP_TRY without a target propagates a result without a value") {
    std::source_location before;
    CHECK(skip_typed({}, before).has_value());

    const alp::Result<void, FileError> result = skip_typed(alp::make_unexpected(FileError::NotFound), before);
    REQUIRE(!result.has_value());
    REQUIRE(result.error().frames().size() == 2);
    CHECK(is_recorded_after(result.error().frames()[1], before));
}

TEST_CASE("ALP_TRY_CONTEXT adds one frame with its message at its line") {
    uint32_t messages = 0;
    std::source_location before;
    const alp::Result<int, FileError> result = forward_with_context(alp::make_unexpected(FileError::NotFound), messages, before);

    REQUIRE(!result.has_value());
    CHECK(messages == 1);
    REQUIRE(result.error().frames().size() == 2);
    CHECK(result.error().frames()[1].message == "loading the configuration");
    CHECK(is_recorded_after(result.error().frames()[1], before));
}

TEST_CASE("ALP_TRY_CONTEXT builds its message only on failure") {
    uint32_t messages = 0;
    std::source_location before;

    CHECK(forward_with_context(3, messages, before).value() == 4);
    CHECK(skip_with_context({}, messages, before).has_value());
    CHECK(messages == 0);
}

TEST_CASE("ALP_TRY_CONTEXT without a target propagates with its message") {
    uint32_t messages = 0;
    std::source_location before;
    const alp::Result<void, FileError> result = skip_with_context(alp::make_unexpected(FileError::NotFound), messages, before);

    REQUIRE(!result.has_value());
    CHECK(messages == 1);
    REQUIRE(result.error().frames().size() == 2);
    CHECK(result.error().frames()[1].message == "loading the configuration");
    CHECK(is_recorded_after(result.error().frames()[1], before));
}

TEST_CASE("ALP_TRY hands a plain code to an enclosing function whose error is that code") {
    CHECK(forward_code_to_code(3).value() == 4);

    const tl::expected<int, FileError> result = forward_code_to_code(tl::make_unexpected(FileError::NotFound));
    REQUIRE(!result.has_value());
    CHECK(result.error() == FileError::NotFound);
}

TEST_CASE("ALP_TRY hands a plain code to an enclosing function whose error is built from it") {
    CHECK(forward_code_to_wrapper(3).value() == 4);

    const tl::expected<int, FileErrorWrapper> result = forward_code_to_wrapper(tl::make_unexpected(FileError::NotFound));
    REQUIRE(!result.has_value());
    CHECK(result.error().code == FileError::NotFound);
}

TEST_CASE("ALP_TRY roots a plain code at its line for an enclosing function that returns an Error") {
    std::source_location before;
    CHECK(forward_code_to_typed(3, before).value() == 4);

    const alp::Result<int, FileError> result = forward_code_to_typed(tl::make_unexpected(FileError::NotFound), before);
    REQUIRE(!result.has_value());
    CHECK(result.error().code() == FileError::NotFound);
    REQUIRE(result.error().frames().size() == 1);
    CHECK(is_recorded_after(result.error().frames()[0], before));
}

TEST_CASE("ALP_TRY roots a plain code at its line for an erased enclosing function") {
    std::source_location before;
    const alp::Result<int> result = forward_code_to_erased(tl::make_unexpected(FileError::NotFound), before);

    REQUIRE(!result.has_value());
    CHECK(fmt::format("{}", result.error()) == "file not found");
    REQUIRE(result.error().frames().size() == 1);
    CHECK(is_recorded_after(result.error().frames()[0], before));
}

TEST_CASE("ALP_TRY_CONTEXT adds its message to a plain code as the first context") {
    std::source_location before;
    const alp::Result<int, FileError> result = forward_code_with_context(tl::make_unexpected(FileError::NotFound), before);

    REQUIRE(!result.has_value());
    CHECK(result.error().code() == FileError::NotFound);
    REQUIRE(result.error().frames().size() == 2);
    CHECK(!result.error().frames()[0].message.has_value());
    CHECK(result.error().frames()[1].message == "reading app.cfg");
    CHECK(is_recorded_after(result.error().frames()[0], before));
    CHECK(is_recorded_after(result.error().frames()[1], before));
}

TEST_CASE("ALP_TRY_CONTEXT hands a plain code without its message to an enclosing function whose error is that code") {
    const tl::expected<int, FileError> result = forward_code_with_context_to_code(tl::make_unexpected(FileError::NotFound));

    REQUIRE(!result.has_value());
    CHECK(result.error() == FileError::NotFound);
}

TEST_CASE("ALP_TRY passes any other error through unchanged") {
    CHECK(forward_string(3).value() == 4);

    const tl::expected<int, std::string> string_result = forward_string(tl::make_unexpected(std::string("broken")));
    REQUIRE(!string_result.has_value());
    CHECK(string_result.error() == "broken");

    const tl::expected<int, std::variant<FileError, Cancelled>> variant_result = forward_variant(tl::make_unexpected(std::variant<FileError, Cancelled>(Cancelled{})));
    REQUIRE(!variant_result.has_value());
    CHECK(std::holds_alternative<Cancelled>(variant_result.error()));
}

TEST_CASE("ALP_TRY propagates an empty optional") {
    CHECK(forward_optional(3).value() == 4);
    CHECK(!forward_optional(std::nullopt).has_value());
    CHECK(skip_optional(3).has_value());
    CHECK(!skip_optional(std::nullopt).has_value());
}

TEST_CASE("ALP_TRY assigns to an existing variable") {
    CHECK(assign_to_existing(3).value() == 3);
    CHECK(!assign_to_existing(alp::make_unexpected(FileError::NotFound)).has_value());
}

TEST_CASE("ALP_TRY assigns to a member and leaves it alone on failure") {
    Holder holder;

    REQUIRE(holder.load(3).has_value());
    CHECK(holder.value == 3);

    CHECK(!holder.load(alp::make_unexpected(FileError::NotFound)).has_value());
    CHECK(holder.value == 3);
}

TEST_CASE("ALP_TRY moves the value out instead of copying it") {
    const alp::Result<Tracked, FileError> result = forward_tracked(Tracked{});

    REQUIRE(result.has_value());
    CHECK(result->copies == 0);
    CHECK(result->moves > 0);
}

TEST_CASE("ALP_TRY binds a reference into the result without copying") {
    CHECK(count_copies_behind_reference(Tracked{}).value() == 0);
}

TEST_CASE("ALP_TRY binds a move-only value") {
    const alp::Result<std::unique_ptr<int>, FileError> result = forward_move_only(std::make_unique<int>(5));

    REQUIRE(result.has_value());
    CHECK(**result == 5);
}

TEST_CASE("ALP_TRY binds a structured binding with one name") {
    CHECK(unpack_single(Single{.value = 4}).value() == 4);
}

TEST_CASE("ALP_TRY evaluates its expression once") {
    uint32_t calls = 0;
    CHECK(forward_counted(calls, 7).value() == 7);
    CHECK(calls == 1);

    calls = 0;
    CHECK(!forward_counted(calls, alp::make_unexpected(FileError::NotFound)).has_value());
    CHECK(calls == 1);
}

TEST_CASE("ALP_TRY in a loop returns at the first failure and leaves the result it was given intact") {
    const std::vector<alp::Result<int, FileError>> sources{
        alp::Result<int, FileError>(1),
        alp::Result<int, FileError>(alp::make_unexpected(FileError::NotFound)),
        alp::Result<int, FileError>(3),
    };
    uint32_t visited = 0;
    const alp::Result<int, FileError> result = sum_until_failure(sources, visited);

    REQUIRE(!result.has_value());
    CHECK(visited == 2);
    CHECK(result.error().frames().size() == 2);
    REQUIRE(!sources[1].has_value());
    CHECK(sources[1].error().frames().size() == 1);
}

TEST_CASE("Two ALP_TRY on one line do not collide") {
    CHECK(add_on_one_line(1, 2).value() == 3);
    CHECK(!add_on_one_line(1, alp::make_unexpected(FileError::NotFound)).has_value());
}

TEST_CASE("ALP_TRY takes an expression with commas inside parentheses") {
    CHECK(add_pair().value() == 3);
}

TEST_CASE("ALP_TRY_CONTEXT takes an expression with commas inside parentheses") {
    CHECK(add_pair_with_context().value() == 3);
}

TEST_CASE("ALP_TRY_CONTEXT takes a message with commas and variables named like its parameters") {
    CHECK(forward_with_parameter_names(3).value() == 3);

    const alp::Result<int, FileError> result = forward_with_parameter_names(alp::make_unexpected(FileError::NotFound));
    REQUIRE(!result.has_value());
    REQUIRE(result.error().frames().size() == 2);
    CHECK(result.error().frames()[1].message == "reading app.cfg, then the index");
}

TEST_CASE("ALP_TRY works inside a lambda") {
    const auto forward = [](alp::Result<int, FileError> source) -> alp::Result<int, FileError> {
        ALP_TRY(const int value, std::move(source));
        return value + 1;
    };

    CHECK(forward(3).value() == 4);
    const alp::Result<int, FileError> result = forward(alp::make_unexpected(FileError::NotFound));
    REQUIRE(!result.has_value());
    CHECK(result.error().frames().size() == 2);
}

TEST_CASE("Frames accumulate across several ALP_TRY") {
    const alp::Result<int> result = fail_outer();

    REQUIRE(!result.has_value());
    REQUIRE(result.error().frames().size() == 3);
    CHECK(result.error().frames()[1].message == "reading the configuration");
    CHECK(!result.error().frames()[2].message.has_value());
    CHECK(fmt::format("{}", result.error()) == "reading the configuration: file not found");
}

TEST_CASE("ALP_TRY widens an error into the enclosing function's set with a frame at its line") {
    std::source_location before;
    CHECK(forward_into_set(3, before).value() == 4);

    const alp::Result<int, FileError, ParseError> result = forward_into_set(alp::make_unexpected(FileError::NotFound), before);
    REQUIRE(!result.has_value());
    CHECK(fmt::format("{}", result.error()) == "file not found");
    REQUIRE(result.error().frames().size() == 2);
    CHECK(is_recorded_after(result.error().frames()[1], before));
}

TEST_CASE("ALP_TRY roots a plain code in the enclosing function's set at its line") {
    std::source_location before;
    CHECK(forward_code_into_set(3, before).value() == 4);

    const alp::Result<int, FileError, ParseError> result = forward_code_into_set(tl::make_unexpected(ParseError::BadDigit), before);
    REQUIRE(!result.has_value());
    CHECK(result.error().narrow<ParseError>().has_value());
    REQUIRE(result.error().frames().size() == 1);
    CHECK(is_recorded_after(result.error().frames()[0], before));
}
