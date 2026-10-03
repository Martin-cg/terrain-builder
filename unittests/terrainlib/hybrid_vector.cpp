#include <memory>
#include <span>
#include <stdexcept>
#include <vector>

#include "../catch2_helpers.h"

#include "containers/HybridVector.h"

TEST_CASE("HybridVector default construction", "[containers][HybridVector]") {
    const HybridVector<int, 3> vec;

    CHECK(vec.size() == 0);
    CHECK(vec.capacity() == 3);
    CHECK(vec.empty());
    CHECK_FALSE(vec.spilled());
}

TEST_CASE("HybridVector initializer_list construction", "[containers][HybridVector]") {
    SECTION("as many values as the inline capacity") {
        const HybridVector<int, 3> vec{1, 2, 3};

        REQUIRE(vec.size() == 3);
        CHECK_FALSE(vec.spilled());
        CHECK(vec[0] == 1);
        CHECK(vec[1] == 2);
        CHECK(vec[2] == 3);
    }

    SECTION("more values than the inline capacity") {
        const HybridVector<int, 3> vec{1, 2, 3, 4};

        REQUIRE(vec.size() == 4);
        CHECK(vec.spilled());
        CHECK(vec[0] == 1);
        CHECK(vec[3] == 4);
    }
}

TEST_CASE("HybridVector push_back keeps values in the inline storage up to its capacity", "[containers][HybridVector]") {
    HybridVector<int, 3> vec;

    vec.push_back(1);
    vec.push_back(2);
    vec.push_back(3);

    REQUIRE(vec.size() == 3);
    CHECK_FALSE(vec.spilled());
    CHECK(vec.capacity() == 3);
    CHECK(vec[0] == 1);
    CHECK(vec[1] == 2);
    CHECK(vec[2] == 3);
}

TEST_CASE("HybridVector push_back spills to the heap past the inline capacity", "[containers][HybridVector]") {
    HybridVector<int, 3> vec{1, 2, 3};

    vec.push_back(4);

    REQUIRE(vec.size() == 4);
    CHECK(vec.spilled());
    CHECK(vec.capacity() > 3);
    CHECK(vec[0] == 1);
    CHECK(vec[1] == 2);
    CHECK(vec[2] == 3);
    CHECK(vec[3] == 4);
}

TEST_CASE("HybridVector emplace_back constructs move-only elements", "[containers][HybridVector]") {
    HybridVector<std::unique_ptr<int>, 2> vec;

    vec.emplace_back(std::make_unique<int>(42));
    vec.emplace_back(std::make_unique<int>(99));

    REQUIRE(vec.size() == 2);
    REQUIRE_FALSE(vec.spilled());
    CHECK(*vec[0] == 42);
    CHECK(*vec[1] == 99);

    vec.emplace_back(std::make_unique<int>(123));

    REQUIRE(vec.size() == 3);
    REQUIRE(vec.spilled());
    CHECK(*vec[0] == 42);
    CHECK(*vec[1] == 99);
    CHECK(*vec[2] == 123);
}

TEST_CASE("HybridVector pop_back removes the last element", "[containers][HybridVector]") {
    SECTION("from the inline storage") {
        HybridVector<int, 3> vec{1, 2, 3};

        vec.pop_back();

        REQUIRE(vec.size() == 2);
        CHECK_FALSE(vec.spilled());
        CHECK(vec[0] == 1);
        CHECK(vec[1] == 2);
    }

    SECTION("from the heap") {
        HybridVector<int, 3> vec{1, 2, 3, 4};

        vec.pop_back();

        REQUIRE(vec.size() == 3);
        CHECK(vec.spilled());
        CHECK(vec[2] == 3);
    }

    SECTION("on an empty vector") {
        HybridVector<int, 3> vec;

        REQUIRE_THROWS_AS(vec.pop_back(), std::out_of_range);
    }
}

TEST_CASE("HybridVector clear removes every element", "[containers][HybridVector]") {
    SECTION("from the inline storage") {
        HybridVector<int, 3> vec{1, 2, 3};

        vec.clear();

        CHECK(vec.empty());
        CHECK(vec.size() == 0);
        CHECK_FALSE(vec.spilled());
    }

    SECTION("from the heap, which it stays on") {
        HybridVector<int, 3> vec{1, 2, 3, 4};

        vec.clear();

        CHECK(vec.empty());
        CHECK(vec.size() == 0);
        CHECK(vec.spilled());
    }
}

TEST_CASE("HybridVector reserve spills when asked for more than the inline capacity", "[containers][HybridVector]") {
    SECTION("up to the inline capacity") {
        HybridVector<int, 3> vec;

        vec.reserve(3);

        CHECK_FALSE(vec.spilled());
        CHECK(vec.capacity() == 3);
    }

    SECTION("beyond the inline capacity") {
        HybridVector<int, 3> vec{1, 2};

        vec.reserve(10);

        REQUIRE(vec.size() == 2);
        CHECK(vec.spilled());
        CHECK(vec.capacity() >= 10);
        CHECK(vec[0] == 1);
        CHECK(vec[1] == 2);
    }
}

TEST_CASE("HybridVector resize value-initialises new elements", "[containers][HybridVector]") {
    SECTION("within the inline capacity") {
        HybridVector<int, 3> vec;

        vec.resize(2);

        REQUIRE(vec.size() == 2);
        CHECK_FALSE(vec.spilled());
        CHECK(vec[0] == 0);
        CHECK(vec[1] == 0);
    }

    SECTION("past the inline capacity") {
        HybridVector<int, 3> vec{1, 2, 3};

        vec.resize(5);

        REQUIRE(vec.size() == 5);
        CHECK(vec.spilled());
        CHECK(vec[0] == 1);
        CHECK(vec[1] == 2);
        CHECK(vec[2] == 3);
        CHECK(vec[3] == 0);
        CHECK(vec[4] == 0);
    }

    SECTION("to fewer elements") {
        HybridVector<int, 3> vec{1, 2, 3};

        vec.resize(1);

        REQUIRE(vec.size() == 1);
        CHECK_FALSE(vec.spilled());
        CHECK(vec[0] == 1);
    }
}

TEST_CASE("HybridVector at throws past the last element", "[containers][HybridVector]") {
    SECTION("in the inline storage") {
        HybridVector<int, 3> vec{1, 2, 3};

        CHECK(vec.at(0) == 1);
        CHECK(vec.at(2) == 3);
        REQUIRE_THROWS_AS(vec.at(3), std::out_of_range);
    }

    SECTION("on the heap") {
        HybridVector<int, 3> vec{1, 2, 3, 4};

        REQUIRE(vec.spilled());
        CHECK(vec.at(0) == 1);
        CHECK(vec.at(3) == 4);
        REQUIRE_THROWS_AS(vec.at(4), std::out_of_range);
    }
}

TEST_CASE("HybridVector operator[] reads and writes every element", "[containers][HybridVector]") {
    SECTION("in the inline storage") {
        HybridVector<int, 3> vec{1, 2, 3};

        vec[1] = 20;

        CHECK(vec[0] == 1);
        CHECK(vec[1] == 20);
        CHECK(vec[2] == 3);
    }

    SECTION("on the heap") {
        HybridVector<int, 3> vec{1, 2, 3, 4};

        vec[3] = 40;

        CHECK(vec[0] == 1);
        CHECK(vec[3] == 40);
    }
}

TEST_CASE("HybridVector iterates over its elements in order", "[containers][HybridVector]") {
    SECTION("in the inline storage") {
        const HybridVector<int, 3> vec{1, 2, 3};

        std::vector<int> visited;
        for (const int value : vec) {
            visited.push_back(value);
        }

        std::vector<int> visited_const;
        for (auto it = vec.cbegin(); it != vec.cend(); it++) {
            visited_const.push_back(*it);
        }

        CHECK(visited == std::vector<int>{1, 2, 3});
        CHECK(visited_const == std::vector<int>{1, 2, 3});
        CHECK(*vec.data() == 1);
    }

    SECTION("on the heap") {
        const HybridVector<int, 3> vec{1, 2, 3, 4};

        std::vector<int> visited;
        for (const int value : vec) {
            visited.push_back(value);
        }

        std::vector<int> visited_const;
        for (auto it = vec.cbegin(); it != vec.cend(); it++) {
            visited_const.push_back(*it);
        }

        CHECK(visited == std::vector<int>{1, 2, 3, 4});
        CHECK(visited_const == std::vector<int>{1, 2, 3, 4});
        CHECK(*vec.data() == 1);
    }
}

TEST_CASE("HybridVector converts to a span of its elements", "[containers][HybridVector]") {
    SECTION("in the inline storage") {
        HybridVector<int, 3> vec{1, 2, 3};

        const std::span<int> values = vec;
        REQUIRE(values.size() == 3);
        CHECK(std::vector<int>(values.begin(), values.end()) == std::vector<int>{1, 2, 3});

        values[0] = 10;
        CHECK(vec[0] == 10);

        const std::span<const int> const_values = vec;
        CHECK(std::vector<int>(const_values.begin(), const_values.end()) == std::vector<int>{10, 2, 3});
    }

    SECTION("on the heap") {
        HybridVector<int, 3> vec{1, 2, 3, 4};

        const std::span<int> values = vec;
        REQUIRE(values.size() == 4);
        CHECK(std::vector<int>(values.begin(), values.end()) == std::vector<int>{1, 2, 3, 4});

        values[3] = 40;
        CHECK(vec[3] == 40);

        const std::span<const int> const_values = vec;
        CHECK(std::vector<int>(const_values.begin(), const_values.end()) == std::vector<int>{1, 2, 3, 40});
    }
}

TEST_CASE("HybridVector compares equal when the elements match", "[containers][HybridVector]") {
    const HybridVector<int, 3> vec{1, 2, 3};

    HybridVector<int, 3> spilled_vec{1, 2, 3};
    spilled_vec.reserve(10);
    REQUIRE(spilled_vec.spilled());

    CHECK(vec == HybridVector<int, 3>{1, 2, 3});
    CHECK(vec == spilled_vec);
    CHECK_FALSE(vec == HybridVector<int, 3>{1, 2, 4});
    CHECK_FALSE(vec == HybridVector<int, 3>{1, 2});
}
