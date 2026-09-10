#include "../catch2_helpers.h"

#include "glm_utils.h"

TEST_CASE("transform applies an operation to every component", "[glm_utils]") {
    const glm::ivec3 v = {1, 2, 3};

    CHECK(transform(v, [](const auto& c) { return c * 2; }) == glm::ivec3{2, 4, 6});
}

TEST_CASE("transform changes the component type to the operation's result", "[glm_utils]") {
    const glm::ivec2 v = {1, 2};
    const auto halved = transform(v, [](const auto& c) { return c / 2.0; });

    STATIC_CHECK(std::is_same_v<decltype(halved), const glm::dvec2>);
    CHECK(halved == glm::dvec2{0.5, 1.0});
}

TEST_CASE("transform pairs components of two vectors", "[glm_utils]") {
    const glm::ivec3 lhs = {1, 2, 3};
    const glm::ivec3 rhs = {10, 20, 30};

    CHECK(transform(lhs, rhs, [](const auto& a, const auto& b) { return a + b; }) == glm::ivec3{11, 22, 33});
}

TEST_CASE("transform pairs components of differently typed vectors", "[glm_utils]") {
    const glm::ivec2 lhs = {1, 2};
    const glm::dvec2 rhs = {0.5, 0.25};
    const auto scaled = transform(lhs, rhs, [](const auto& a, const auto& b) { return a * b; });

    STATIC_CHECK(std::is_same_v<decltype(scaled), const glm::dvec2>);
    CHECK(scaled == glm::dvec2{0.5, 0.5});
}