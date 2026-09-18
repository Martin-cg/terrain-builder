#include <cstdint>

#include <glm/glm.hpp>

#include "../catch2_helpers.h"

#include "numeric/checked_cast.h"

TEST_CASE("checked_cast keeps every value the target type can hold", "[checked_cast]") {
    CHECK(checked_cast<uint8_t>(int64_t{255}) == 255);
    CHECK(checked_cast<uint8_t>(int64_t{0}) == 0);
    CHECK(checked_cast<int32_t>(uint64_t{2147483647}) == 2147483647);
    CHECK(checked_cast<int8_t>(int64_t{-128}) == -128);
    CHECK(checked_cast<uint64_t>(int8_t{127}) == 127);
    CHECK(checked_cast<int64_t>(uint32_t{4294967295}) == 4294967295);
}

TEST_CASE("checked_cast rejects every value the target type cannot hold", "[checked_cast]") {
    CHECK_THROWS(checked_cast<uint8_t>(int64_t{256}));
    CHECK_THROWS(checked_cast<uint8_t>(int64_t{-1}));
    CHECK_THROWS(checked_cast<int32_t>(uint64_t{2147483648}));
    CHECK_THROWS(checked_cast<int8_t>(int64_t{-129}));
    CHECK_THROWS(checked_cast<uint64_t>(int8_t{-1}));
    CHECK_THROWS(checked_cast<uint32_t>(int64_t{4294967296}));
}

TEST_CASE("debug_checked_cast keeps every value the target type can hold", "[checked_cast]") {
    CHECK(debug_checked_cast<uint8_t>(int64_t{255}) == 255);
    CHECK(debug_checked_cast<uint8_t>(int64_t{0}) == 0);
    CHECK(debug_checked_cast<int32_t>(uint64_t{2147483647}) == 2147483647);
    CHECK(debug_checked_cast<int8_t>(int64_t{-128}) == -128);
    CHECK(debug_checked_cast<uint64_t>(int8_t{127}) == 127);
    CHECK(debug_checked_cast<int64_t>(uint32_t{4294967295}) == 4294967295);
}

#ifndef NDEBUG
TEST_CASE("debug_checked_cast rejects every value the target type cannot hold", "[checked_cast]") {
    CHECK_THROWS(debug_checked_cast<uint8_t>(int64_t{256}));
    CHECK_THROWS(debug_checked_cast<uint8_t>(int64_t{-1}));
    CHECK_THROWS(debug_checked_cast<int32_t>(uint64_t{2147483648}));
    CHECK_THROWS(debug_checked_cast<int8_t>(int64_t{-129}));
    CHECK_THROWS(debug_checked_cast<uint64_t>(int8_t{-1}));
    CHECK_THROWS(debug_checked_cast<uint32_t>(int64_t{4294967296}));
}
#endif

TEST_CASE("checked_cast converts a vector whose components all fit", "[checked_cast]") {
    CHECK(checked_cast<uint8_t>(glm::i64vec3{0, 128, 255}) == glm::u8vec3{0, 128, 255});
    CHECK(checked_cast<int8_t>(glm::u64vec2{0, 127}) == glm::i8vec2{0, 127});
}

TEST_CASE("checked_cast rejects a vector with a single component that does not fit", "[checked_cast]") {
    CHECK_THROWS(checked_cast<uint8_t>(glm::i64vec3{-1, 0, 0}));
    CHECK_THROWS(checked_cast<uint8_t>(glm::i64vec3{0, 0, 256}));
}

TEST_CASE("debug_checked_cast converts a vector whose components all fit", "[checked_cast]") {
    CHECK(debug_checked_cast<uint8_t>(glm::i64vec3{0, 128, 255}) == glm::u8vec3{0, 128, 255});
    CHECK(debug_checked_cast<int8_t>(glm::u64vec2{0, 127}) == glm::i8vec2{0, 127});
}

#ifndef NDEBUG
TEST_CASE("debug_checked_cast rejects a vector with a single component that does not fit", "[checked_cast]") {
    CHECK_THROWS(debug_checked_cast<uint8_t>(glm::i64vec3{-1, 0, 0}));
    CHECK_THROWS(debug_checked_cast<uint8_t>(glm::i64vec3{0, 0, 256}));
}
#endif
