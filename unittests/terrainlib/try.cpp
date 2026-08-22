#include "../catch2_helpers.h"

#include <optional>
#include <string>

#include <tl/expected.hpp>

#include "try.h"

namespace {

using Expected = tl::expected<int, std::string>;

Expected forward_expected(const Expected &source) {
    TRY(const int value, source);
    return value;
}

std::optional<int> forward_optional(const std::optional<int> &source) {
    TRY(const int value, source);
    return value;
}

// Propagates a failure without binding the value it carries.
tl::expected<void, std::string> skip_expected(const Expected &source) {
    TRY(source);
    return {};
}

std::optional<int> skip_optional(const std::optional<int> &source) {
    TRY(source);
    return 0;
}

// Counts how it was handed around, so a copy on the way out of TRY shows up.
struct Tracked {
    int copies = 0;
    int moves = 0;

    Tracked() = default;
    Tracked(const Tracked &other) : copies(other.copies + 1), moves(other.moves) {}
    Tracked(Tracked &&other) noexcept : copies(other.copies), moves(other.moves + 1) {}
    Tracked &operator=(const Tracked &) = delete;
    Tracked &operator=(Tracked &&) = delete;
};

std::optional<Tracked> forward_tracked(std::optional<Tracked> source) {
    TRY(Tracked value, std::move(source));
    return value;
}

int evaluations = 0;

std::optional<int> counted(const std::optional<int> value) {
    evaluations++;
    return value;
}

std::optional<int> forward_counted(const std::optional<int> value) {
    TRY(const int unwrapped, counted(value));
    return unwrapped;
}

} // namespace

TEST_CASE("TRY takes the value of a succeeding expected") {
    const Expected result = forward_expected(Expected{21});
    REQUIRE(result.has_value());
    CHECK(*result == 21);
}

TEST_CASE("TRY propagates the error of a failing expected") {
    const Expected result = forward_expected(tl::make_unexpected(std::string("broken")));
    REQUIRE(!result.has_value());
    CHECK(result.error() == "broken");
}

TEST_CASE("TRY propagates an error without binding a value") {
    CHECK(skip_expected(Expected{1}).has_value());

    const tl::expected<void, std::string> result = skip_expected(tl::make_unexpected(std::string("broken")));
    REQUIRE(!result.has_value());
    CHECK(result.error() == "broken");
}

TEST_CASE("TRY takes the value of an engaged optional") {
    const std::optional<int> result = forward_optional(21);
    REQUIRE(result.has_value());
    CHECK(*result == 21);
}

TEST_CASE("TRY propagates an empty optional as empty") {
    CHECK(!forward_optional(std::nullopt).has_value());
    CHECK(!skip_optional(std::nullopt).has_value());
    CHECK(skip_optional(1).has_value());
}

TEST_CASE("TRY moves the value out instead of copying it") {
    const std::optional<Tracked> result = forward_tracked(std::optional<Tracked>{Tracked{}});
    REQUIRE(result.has_value());
    CHECK(result->copies == 0);
    CHECK(result->moves > 0);
}

TEST_CASE("TRY evaluates its expression once") {
    evaluations = 0;
    CHECK(forward_counted(7) == 7);
    CHECK(evaluations == 1);

    evaluations = 0;
    CHECK(!forward_counted(std::nullopt).has_value());
    CHECK(evaluations == 1);
}
