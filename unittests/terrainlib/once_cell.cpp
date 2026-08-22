#include "../catch2_helpers.h"

#include <atomic>
#include <cstdint>
#include <string>
#include <thread>
#include <vector>

#include "containers/OnceCell.h"

TEST_CASE("OnceCell holds nothing before it is set", "[once_cell]") {
    const OnceCell<int> cell;

    CHECK(cell.get() == nullptr);
    CHECK_FALSE(cell.is_set());
}

TEST_CASE("OnceCell keeps the value it was set to", "[once_cell]") {
    OnceCell<int> cell;

    CHECK(cell.set(7));

    REQUIRE(cell.is_set());
    REQUIRE(cell.get() != nullptr);
    CHECK(*cell.get() == 7);
}

TEST_CASE("OnceCell drops every set after the first", "[once_cell]") {
    OnceCell<int> cell;

    REQUIRE(cell.set(1));
    CHECK_FALSE(cell.set(2));
    CHECK_FALSE(cell.set(3));

    CHECK(*cell.get() == 1);
}

TEST_CASE("OnceCell moves the value in instead of copying it", "[once_cell]") {
    OnceCell<std::string> cell;

    std::string value(64, 'x');
    const char *const buffer = value.data();
    REQUIRE(cell.set(std::move(value)));

    CHECK(cell.get()->data() == buffer);
}

TEST_CASE("OnceCell hands the slot to exactly one of many threads", "[once_cell]") {
    constexpr uint32_t thread_count = 8;

    OnceCell<uint32_t> cell;
    std::atomic<uint32_t> winners = 0;

    std::vector<std::thread> setters;
    for (uint32_t i = 0; i < thread_count; i++) {
        setters.emplace_back([&, i] {
            if (cell.set(i)) {
                winners++;
            }
        });
    }
    for (std::thread &setter : setters) {
        setter.join();
    }

    CHECK(winners == 1);
    REQUIRE(cell.is_set());
    CHECK(*cell.get() < thread_count);
}

TEST_CASE("OnceCell never shows a value that is only half written", "[once_cell]") {
    constexpr uint32_t reader_count = 4;
    const std::string expected(4096, 'a');

    OnceCell<std::string> cell;
    std::atomic<bool> stop = false;

    // Catch2 assertions are not thread safe, so the readers only record what they saw.
    std::atomic<bool> saw_a_partial_value = false;
    std::vector<std::thread> readers;
    for (uint32_t i = 0; i < reader_count; i++) {
        readers.emplace_back([&] {
            while (!stop) {
                const std::string *const held = cell.get();
                if (held != nullptr && *held != expected) {
                    saw_a_partial_value = true;
                }
            }
        });
    }

    REQUIRE(cell.set(expected));
    stop = true;
    for (std::thread &reader : readers) {
        reader.join();
    }

    CHECK_FALSE(saw_a_partial_value);
    CHECK(*cell.get() == expected);
}
