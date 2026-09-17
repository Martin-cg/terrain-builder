#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <source_location>

#include "error/Trace.h"

TEST_CASE("A trace starts where the error originated and grows innermost first", "[error][Trace]") {
    alp::Trace trace(std::source_location::current());
    trace.push(alp::Frame{.message = "outer", .location = std::source_location::current()});

    REQUIRE(trace.frames().size() == 2);
    CHECK(trace.frames()[0].message == std::nullopt);
    CHECK(trace.frames()[1].message == "outer");
}