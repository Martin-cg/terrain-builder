#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "../catch2_helpers.h"

#include "containers/Buffer.h"

TEST_CASE("make_buffer keeps a buffer below MAX_STACK_BYTES on the stack", "[containers][Buffer]") {
    const auto buffer = make_buffer<std::byte>(make_size<1023>());

    STATIC_REQUIRE(std::is_same_v<decltype(buffer), const Buffer<std::byte, ComptimeSize<1023>>>);
}

TEST_CASE("make_buffer moves a buffer of exactly MAX_STACK_BYTES to the heap", "[containers][Buffer]") {
    const auto buffer = make_buffer<std::byte>(make_size<1024>());

    STATIC_REQUIRE(std::is_same_v<decltype(buffer), const Buffer<std::byte, RuntimeSize>>);
    REQUIRE(buffer.size() == 1024);
}

TEST_CASE("make_buffer counts element bytes, not elements", "[containers][Buffer]") {
    const auto buffer = make_buffer<uint32_t>(make_size<256>());

    STATIC_REQUIRE(std::is_same_v<decltype(buffer), const Buffer<uint32_t, RuntimeSize>>);
    REQUIRE(buffer.size() == 256);
}

TEST_CASE("make_buffer puts a runtime size on the heap", "[containers][Buffer]") {
    const auto buffer = make_buffer<std::byte>(make_size(3));

    STATIC_REQUIRE(std::is_same_v<decltype(buffer), const Buffer<std::byte, RuntimeSize>>);
    REQUIRE(buffer.size() == 3);
}