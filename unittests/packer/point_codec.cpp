#include <cstdint>
#include <glm/glm.hpp>

#include "../catch2_helpers.h"
#include "PointCodec.h"

namespace packer {

using Codec = PointCodec<3, int64_t>;

TEST_CASE("PointCodec encodes points relative to the anchor", "[PointCodec]") {
    const Codec codec{{100, 200, 300}, {}};

    CHECK(codec.encode({100, 200, 300}) == glm::u64vec3{0, 0, 0});
    CHECK(codec.encode({101, 200, 305}) == glm::u64vec3{1, 0, 5});
    CHECK(codec.decode({1, 0, 5}) == glm::i64vec3{101, 200, 305});
}

TEST_CASE("PointCodec round-trips every point at exponent zero", "[PointCodec]") {
    const Codec codec{{-1000, 0, 1 << 20}, {}};

    for (int64_t offset = 0; offset < 1024; offset++) {
        const glm::i64vec3 p = {-1000 + offset, offset * 3, (1 << 20) + offset};
        REQUIRE(codec.decode(codec.encode(p)) == p);
    }
}

TEST_CASE("PointCodec applies a separate exponent per axis", "[PointCodec]") {
    const Codec codec{{0, 0, 0}, {0, 1, 9}};

    CHECK(codec.decode({1, 1, 1}) == glm::i64vec3{1, 2, 512});
    CHECK(codec.encode({1, 2, 512}) == glm::u64vec3{1, 1, 1});
}

TEST_CASE("PointCodec rounds a point between coordinates in the requested direction", "[PointCodec]") {
    const Codec codec{{0, 0, 0}, {9, 9, 9}};

    // Half a step, so the three directions disagree.
    const glm::i64vec3 p = {256, 256, 256};

    CHECK(codec.encode<std::round_toward_neg_infinity>(p) == glm::u64vec3{0, 0, 0});
    CHECK(codec.encode<std::round_toward_infinity>(p) == glm::u64vec3{1, 1, 1});
    CHECK(codec.encode<std::round_to_nearest>(p) == glm::u64vec3{1, 1, 1});

    // Just below half stays down under nearest, and still rises under the upward direction.
    const glm::i64vec3 below = {255, 255, 255};
    CHECK(codec.encode<std::round_to_nearest>(below) == glm::u64vec3{0, 0, 0});
    CHECK(codec.encode<std::round_toward_infinity>(below) == glm::u64vec3{1, 1, 1});
}

TEST_CASE("PointCodec brackets a box by rounding its corners outward", "[PointCodec]") {
    const Codec codec{{0, 0, 0}, {9, 9, 9}};

    const glm::i64vec3 minimum = {600, 100, 1100};
    const glm::i64vec3 maximum = {1500, 900, 1600};

    const glm::u64vec3 low = codec.encode<std::round_toward_neg_infinity>(minimum);
    const glm::u64vec3 high = codec.encode<std::round_toward_infinity>(maximum);

    // The decoded box contains the true one on every axis, which is what conservativeness rests on.
    CHECK(glm::all(glm::lessThanEqual(codec.decode(low), minimum)));
    CHECK(glm::all(glm::greaterThanEqual(codec.decode(high), maximum)));
}

}
