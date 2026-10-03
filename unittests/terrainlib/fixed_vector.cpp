#include "../catch2_helpers.h"
#include "containers/FixedVector.h"

#include <array>
#include <cstdint>
#include <memory>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {

// Shares a token so live elements show up in its use count, and throws when an element marked as poisoned is copied.
struct Element {
    std::shared_ptr<int> token;
    bool poisoned = false;

    Element(std::shared_ptr<int> token, const bool poisoned) : token(std::move(token)), poisoned(poisoned) {}
    Element(const Element &other) : token(other.token), poisoned(other.poisoned) {
        if (other.poisoned) {
            throw std::runtime_error("copy failed");
        }
    }
    Element(Element &&) = default;
    Element &operator=(const Element &) = default;
    Element &operator=(Element &&) = default;
};

}

TEST_CASE("FixedVector default construction", "[containers][FixedVector]") {
    FixedVector<int, 5> vec;
    CHECK(vec.size() == 0);
    CHECK(vec.capacity() == 5);
    CHECK(vec.empty());
    CHECK(!vec.full());
}

TEST_CASE("FixedVector initializer_list construction", "[containers][FixedVector]") {
    FixedVector<int, 5> vec{1, 2, 3};
    REQUIRE(vec.size() == 3);
    CHECK(vec[0] == 1);
    CHECK(vec[1] == 2);
    CHECK(vec[2] == 3);

    REQUIRE_THROWS_AS((FixedVector<int, 2>{1, 2, 3}), std::out_of_range);
}

TEST_CASE("FixedVector compares equal when size and elements match", "[containers][FixedVector]") {
    const FixedVector<int, 4> vec{1, 2, 3};

    CHECK(vec == FixedVector<int, 4>{1, 2, 3});
    CHECK_FALSE(vec == FixedVector<int, 4>{1, 2, 4});
    CHECK_FALSE(vec == FixedVector<int, 4>{1, 2});
}

TEST_CASE("FixedVector copy construction copies every element", "[containers][FixedVector]") {
    const FixedVector<int, 5> source{1, 2, 3};

    const FixedVector<int, 5> copy(source);

    CHECK(copy == FixedVector<int, 5>{1, 2, 3});
}

TEST_CASE("FixedVector copy assignment replaces the contents", "[containers][FixedVector]") {
    const FixedVector<int, 5> source{1, 2, 3};

    SECTION("empty target") {
        FixedVector<int, 5> target;
        target = source;
        CHECK(target == source);
    }
    SECTION("shorter target") {
        FixedVector<int, 5> target{7};
        target = source;
        CHECK(target == source);
    }
    SECTION("equally long target") {
        FixedVector<int, 5> target{7, 8, 9};
        target = source;
        CHECK(target == source);
    }
    SECTION("longer target") {
        FixedVector<int, 5> target{7, 8, 9, 10, 11};
        target = source;
        CHECK(target == source);
    }
    SECTION("itself") {
        FixedVector<int, 5> target{1, 2, 3};
        // Exercise self-assignment through an alias to avoid Clang's syntactic self-assignment warning.
        const auto *self = &target;
        target = *self;
        CHECK(target == source);
    }
}

TEST_CASE("FixedVector move construction takes every element and empties the source", "[containers][FixedVector]") {
    FixedVector<int, 5> source{1, 2, 3};

    const FixedVector<int, 5> moved(std::move(source));

    CHECK(moved == FixedVector<int, 5>{1, 2, 3});
    CHECK(source.empty());
}

TEST_CASE("FixedVector move assignment replaces the contents and empties the source", "[containers][FixedVector]") {
    FixedVector<int, 5> source{1, 2, 3};

    SECTION("empty target") {
        FixedVector<int, 5> target;
        target = std::move(source);
        CHECK(target == FixedVector<int, 5>{1, 2, 3});
    }
    SECTION("shorter target") {
        FixedVector<int, 5> target{7};
        target = std::move(source);
        CHECK(target == FixedVector<int, 5>{1, 2, 3});
    }
    SECTION("longer target") {
        FixedVector<int, 5> target{7, 8, 9, 10, 11};
        target = std::move(source);
        CHECK(target == FixedVector<int, 5>{1, 2, 3});
    }

    CHECK(source.empty());
}

TEST_CASE("FixedVector emplace_back and push_back", "[containers][FixedVector]") {
    FixedVector<std::pair<int, int>, 3> vec;
    vec.emplace_back(1, 2);
    vec.emplace_back(3, 4);
    REQUIRE(vec.size() == 2);
    CHECK(vec[0].first == 1);
    CHECK(vec[1].second == 4);

    vec.push_back(std::make_pair(5, 6));
    CHECK(vec.size() == 3);
    CHECK(vec.full());
    REQUIRE_THROWS_AS(vec.emplace_back(7, 8), std::out_of_range);
    REQUIRE_THROWS_AS(vec.push_back(std::make_pair(9, 10)), std::out_of_range);
}

TEST_CASE("FixedVector try_emplace_back and try_push_back", "[containers][FixedVector]") {
    FixedVector<int, 2> vec;
    CHECK(vec.try_emplace_back(1));
    CHECK(vec.try_push_back(2));
    REQUIRE_FALSE(vec.try_emplace_back(3));
    REQUIRE_FALSE(vec.try_push_back(4));
    CHECK(vec == FixedVector<int, 2>{1, 2});
}

TEST_CASE("FixedVector pop_back", "[containers][FixedVector]") {
    FixedVector<int, 3> vec{1, 2};
    vec.pop_back();
    REQUIRE(vec.size() == 1);
    CHECK(vec[0] == 1);
    vec.pop_back();
    CHECK(vec.empty());
    REQUIRE_THROWS_AS(vec.pop_back(), std::out_of_range);
}

TEST_CASE("FixedVector back", "[containers][FixedVector]") {
    FixedVector<int, 3> vec{1, 2, 3};
    CHECK(vec.back() == 3);

    vec.clear();
    REQUIRE_THROWS_AS(vec.back(), std::out_of_range);
}

TEST_CASE("FixedVector at() bounds checking", "[containers][FixedVector]") {
    FixedVector<int, 2> vec{5, 6};
    CHECK(vec.at(0) == 5);
    CHECK(vec.at(1) == 6);
    REQUIRE_THROWS_AS(vec.at(2), std::out_of_range);
}

TEST_CASE("FixedVector clear()", "[containers][FixedVector]") {
    FixedVector<int, 3> vec{1, 2, 3};
    vec.clear();
    CHECK(vec.empty());
    CHECK(vec.size() == 0);
    REQUIRE_THROWS_AS(vec.pop_back(), std::out_of_range);
}

TEST_CASE("FixedVector resize", "[containers][FixedVector]") {
    FixedVector<int, 4> vec{1, 2};

    vec.resize(4);
    CHECK(vec == FixedVector<int, 4>{1, 2, 0, 0});

    vec.resize(2);
    CHECK(vec == FixedVector<int, 4>{1, 2});

    REQUIRE_THROWS_AS(vec.resize(5), std::out_of_range);
}

TEST_CASE("FixedVector erase_at", "[containers][FixedVector]") {
    FixedVector<int, 4> vec{1, 2, 3, 4};

    vec.erase_at(1);

    CHECK(vec == FixedVector<int, 4>{1, 3, 4});
    REQUIRE_THROWS_AS(vec.erase_at(3), std::out_of_range);
}

TEST_CASE("FixedVector erase by iterator", "[containers][FixedVector]") {
    FixedVector<int, 4> vec{1, 2, 3, 4};

    const int *next = vec.erase(vec.begin() + 1);

    CHECK(vec == FixedVector<int, 4>{1, 3, 4});
    CHECK(next == vec.begin() + 1);
    REQUIRE_THROWS_AS(vec.erase(vec.end()), std::out_of_range);
}

TEST_CASE("FixedVector erase a range", "[containers][FixedVector]") {
    FixedVector<int, 5> vec{1, 2, 3, 4, 5};

    SECTION("non-empty range") {
        const int *next = vec.erase(vec.begin() + 1, vec.begin() + 3);

        CHECK(vec == FixedVector<int, 5>{1, 4, 5});
        CHECK(next == vec.begin() + 1);
    }
    SECTION("empty range") {
        const int *next = vec.erase(vec.begin() + 2, vec.begin() + 2);

        CHECK(vec == FixedVector<int, 5>{1, 2, 3, 4, 5});
        CHECK(next == vec.begin() + 2);
    }
    SECTION("reversed range") {
        REQUIRE_THROWS_AS(vec.erase(vec.begin() + 3, vec.begin() + 1), std::out_of_range);
        CHECK(vec == FixedVector<int, 5>{1, 2, 3, 4, 5});
    }
}

TEST_CASE("FixedVector append_range", "[containers][FixedVector]") {
    FixedVector<int, 3> vec{1};

    SECTION("fits") {
        vec.append_range(std::vector<int>{2, 3});
        CHECK(vec == FixedVector<int, 3>{1, 2, 3});
    }
    SECTION("overflows") {
        REQUIRE_THROWS_AS(vec.append_range(std::vector<int>{2, 3, 4}), std::out_of_range);
        CHECK(vec == FixedVector<int, 3>{1});
    }
}

TEST_CASE("FixedVector assignment from a fixed-size span replaces the contents", "[containers][FixedVector]") {
    FixedVector<int, 3> vec{1, 2, 3};
    const std::array<int, 2> values = {7, 8};

    vec = std::span<const int, 2>(values);

    CHECK(vec == FixedVector<int, 3>{7, 8});
}

TEST_CASE("FixedVector begin/end iterators", "[containers][FixedVector]") {
    FixedVector<int, 3> vec{1, 2, 3};

    std::vector<int> values;
    for (auto it = vec.begin(); it != vec.end(); ++it) {
        values.push_back(*it);
    }
    CHECK(values == std::vector<int>{1, 2, 3});
}

TEST_CASE("FixedVector std::span conversion", "[containers][FixedVector]") {
    FixedVector<int, 3> vec{1, 2, 3};
    std::span<int> s = vec;
    REQUIRE(s.size() == 3);
    CHECK(s[0] == 1);

    std::span<const int> cs = vec;
    CHECK(cs[1] == 2);
}

TEST_CASE("FixedVector works with a move-only element type", "[containers][FixedVector]") {
    FixedVector<std::unique_ptr<int>, 3> vec;
    vec.emplace_back(std::make_unique<int>(10));
    vec.push_back(std::make_unique<int>(20));

    SECTION("move construction") {
        const FixedVector<std::unique_ptr<int>, 3> moved(std::move(vec));

        REQUIRE(moved.size() == 2);
        CHECK(*moved[0] == 10);
        CHECK(*moved[1] == 20);
    }
    SECTION("move assignment") {
        FixedVector<std::unique_ptr<int>, 3> moved;
        moved.emplace_back(std::make_unique<int>(30));
        moved = std::move(vec);

        REQUIRE(moved.size() == 2);
        CHECK(*moved[0] == 10);
        CHECK(*moved[1] == 20);
    }

    CHECK(vec.empty());
}

TEST_CASE("FixedVector destroys each element exactly once", "[containers][FixedVector]") {
    const auto token = std::make_shared<int>(0);
    {
        FixedVector<std::shared_ptr<int>, 4> vec{token, token, token, token};
        CHECK(token.use_count() == 5);

        vec.pop_back();
        CHECK(token.use_count() == 4);

        vec.erase_at(0);
        CHECK(token.use_count() == 3);

        vec.erase(vec.begin(), vec.begin() + 1);
        CHECK(token.use_count() == 2);

        vec.resize(0);
        CHECK(token.use_count() == 1);

        vec.push_back(token);
        vec.clear();
        CHECK(token.use_count() == 1);

        vec.push_back(token);
    }
    CHECK(token.use_count() == 1);
}

TEST_CASE("FixedVector assignment destroys the surplus of a longer target", "[containers][FixedVector]") {
    const auto token = std::make_shared<int>(0);
    FixedVector<std::shared_ptr<int>, 4> source{token, token};
    FixedVector<std::shared_ptr<int>, 4> target{token, token, token, token};

    SECTION("copy") {
        target = source;

        CHECK(target.size() == 2);
        CHECK(token.use_count() == 5);
    }
    SECTION("move") {
        target = std::move(source);

        CHECK(target.size() == 2);
        CHECK(token.use_count() == 3);
    }
}

TEST_CASE("FixedVector destroys what it built when copying an element throws", "[containers][FixedVector]") {
    const auto token = std::make_shared<int>(0);
    FixedVector<Element, 4> source;
    source.emplace_back(token, false);
    source.emplace_back(token, false);
    source.emplace_back(token, true);

    SECTION("copy construction") {
        REQUIRE_THROWS_AS((FixedVector<Element, 4>(source)), std::runtime_error);

        CHECK(token.use_count() == 4);
    }
    SECTION("copy assignment into a shorter target") {
        FixedVector<Element, 4> target;
        target.emplace_back(token, false);

        REQUIRE_THROWS_AS(target = source, std::runtime_error);

        CHECK(target.size() == 1);
        CHECK(token.use_count() == 5);
    }
}