#include "../catch2_helpers.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <functional>
#include <iterator>
#include <memory>
#include <random>
#include <vector>

#include "build/MergingWriter.h"

namespace {

template <size_t K, typename T = int, typename Compare = std::less<>>
auto collect_into(std::vector<T> &out, Compare compare = {}) {
    auto sink = [&out](T value) {
        out.push_back(std::move(value));
    };
    return MergingWriter<T, K, decltype(sink), Compare>(std::move(sink), std::move(compare));
}

} // namespace

TEST_CASE("a lone source streams straight through", "[build][MergingWriter]") {
    std::vector<int> out;
    auto writer = collect_into<1>(out);

    writer.push(0, 1);
    writer.push(0, 2);
    CHECK(out == std::vector<int>{1, 2});

    writer.complete(0);
    CHECK(out == std::vector<int>{1, 2});
}

TEST_CASE("nothing is emitted while a source is still silent", "[build][MergingWriter]") {
    std::vector<int> out;
    auto writer = collect_into<2>(out);

    writer.push(0, 1);
    writer.push(0, 2);
    CHECK(out.empty());
}

TEST_CASE("completing a silent source releases what the others queued", "[build][MergingWriter]") {
    std::vector<int> out;
    auto writer = collect_into<2>(out);

    writer.push(0, 1);
    writer.push(0, 2);
    writer.complete(1);
    CHECK(out == std::vector<int>{1, 2});

    writer.complete(0);
    CHECK(out == std::vector<int>{1, 2});
}

TEST_CASE("two sources interleave in sorted order", "[build][MergingWriter]") {
    std::vector<int> out;
    auto writer = collect_into<2>(out);

    writer.push(0, 1);
    writer.push(1, 2);
    CHECK(out == std::vector<int>{1});

    writer.push(0, 3);
    CHECK(out == std::vector<int>{1, 2});

    writer.push(1, 4);
    CHECK(out == std::vector<int>{1, 2, 3});

    writer.complete(0);
    CHECK(out == std::vector<int>{1, 2, 3, 4});

    writer.complete(1);
    CHECK(out == std::vector<int>{1, 2, 3, 4});
}

TEST_CASE("a value is held until no source can still beat it", "[build][MergingWriter]") {
    std::vector<int> out;
    auto writer = collect_into<2>(out);

    writer.push(1, 10);
    CHECK(out.empty());

    // Ten stays put: source zero has gone quiet again and might yet send something smaller.
    writer.push(0, 1);
    CHECK(out == std::vector<int>{1});

    writer.complete(0);
    CHECK(out == std::vector<int>{1, 10});
}

TEST_CASE("the comparator decides the merge order", "[build][MergingWriter]") {
    std::vector<int> out;
    auto writer = collect_into<2>(out, std::greater<>{});

    writer.push(0, 5);
    writer.push(1, 9);
    CHECK(out == std::vector<int>{9});

    writer.push(1, 7);
    CHECK(out == std::vector<int>{9, 7});

    writer.complete(1);
    CHECK(out == std::vector<int>{9, 7, 5});

    writer.complete(0);
    CHECK(out == std::vector<int>{9, 7, 5});
}

TEST_CASE("completing every source without a value emits nothing", "[build][MergingWriter]") {
    std::vector<int> out;
    auto writer = collect_into<3>(out);

    writer.complete(0);
    writer.complete(1);
    writer.complete(2);

    CHECK(out.empty());
}

TEST_CASE("move only values survive the merge", "[build][MergingWriter]") {
    using Value = std::unique_ptr<int>;

    std::vector<Value> out;
    auto writer = collect_into<2, Value>(out, [](const Value &a, const Value &b) {
        return *a < *b;
    });

    writer.push(0, std::make_unique<int>(2));
    writer.push(1, std::make_unique<int>(1));
    writer.complete(0);
    writer.complete(1);

    REQUIRE(out.size() == 2);
    CHECK(*out[0] == 1);
    CHECK(*out[1] == 2);
}

TEST_CASE("end to end, four sorted streams merge into one sorted stream", "[build][MergingWriter]") {
    constexpr size_t source_count = 4;
    constexpr size_t per_source = 200;

    std::mt19937 rng(1234);
    std::uniform_int_distribution<int> values(0, 500);

    std::array<std::vector<int>, source_count> streams;
    std::vector<int> expected;
    for (std::vector<int> &stream : streams) {
        std::ranges::generate_n(std::back_inserter(stream), per_source, [&] {
            return values(rng);
        });
        std::ranges::sort(stream);
        std::ranges::copy(stream, std::back_inserter(expected));
    }
    std::ranges::sort(expected);

    // Shuffled so the merge never sees a tidy round robin.
    std::vector<size_t> order;
    for (size_t source = 0; source < source_count; source++) {
        order.insert(order.end(), per_source, source);
    }
    std::ranges::shuffle(order, rng);

    std::vector<int> out;
    auto writer = collect_into<source_count>(out);

    std::array<size_t, source_count> taken{};
    for (const size_t source : order) {
        writer.push(source, streams[source][taken[source]++]);
    }
    for (size_t source = 0; source < source_count; source++) {
        writer.complete(source);
    }

    CHECK(out == expected);
}
