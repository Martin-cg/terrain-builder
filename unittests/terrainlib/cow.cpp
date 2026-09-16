#include "../catch2_helpers.h"
#include "containers/Cow.h"

#include <cstdint>
#include <memory>
#include <type_traits>
#include <vector>

namespace {

// A value that counts how often it has been copied.
struct CopyCounter {
    explicit CopyCounter(uint32_t &copies) : copies(&copies) {}
    CopyCounter(const CopyCounter &other) : copies(other.copies) {
        (*this->copies)++;
    }
    CopyCounter(CopyCounter &&) = default;
    CopyCounter &operator=(const CopyCounter &other) {
        this->copies = other.copies;
        (*this->copies)++;
        return *this;
    }
    CopyCounter &operator=(CopyCounter &&) = default;

    uint32_t *copies;
};

template <typename T>
concept Cowable = requires { typename Cow<T>; };

template <typename Source>
concept Borrowable = requires(Source &&source) {
    Cow<std::remove_cvref_t<Source>>::borrowed(std::forward<Source>(source));
};

template <typename Source>
concept Ownable = requires(Source &&source) {
    Cow<std::remove_cvref_t<Source>>::owned(std::forward<Source>(source));
};

template <typename Self>
concept Dereferenceable = requires(Self &&cow) { *std::forward<Self>(cow); };

template <typename Self>
concept ArrowAccessible = requires(Self &&cow) { std::forward<Self>(cow).operator->(); };

template <typename Self>
concept MutablyAccessible = requires(Self &&cow) { std::forward<Self>(cow).to_mut(); };

template <typename Self>
concept IntoOwnable = requires(Self &&cow) { std::forward<Self>(cow).into_owned(); };

template <typename Self>
concept Visitable = requires(Self &&cow) { std::forward<Self>(cow).visit([](const auto &) {}); };

}

TEST_CASE("Cow rejects types it cannot hold", "[containers][Cow]") {
    STATIC_REQUIRE(Cowable<std::vector<int>>);
    STATIC_REQUIRE_FALSE(Cowable<const std::vector<int>>);
    STATIC_REQUIRE_FALSE(Cowable<std::vector<int> &>);
    STATIC_REQUIRE_FALSE(Cowable<std::vector<int> *>);
    STATIC_REQUIRE_FALSE(Cowable<std::unique_ptr<int>>);
}

TEST_CASE("Cow::borrowed refers to the source without copying it", "[containers][Cow]") {
    const std::vector<int> source = {1, 2, 3};
    const auto cow = Cow<std::vector<int>>::borrowed(source);

    CHECK(cow.is_borrowed());
    CHECK_FALSE(cow.is_owned());
    CHECK(&*cow == &source);
    CHECK(cow->size() == 3);
}

TEST_CASE("Cow::borrowed rejects temporaries", "[containers][Cow]") {
    STATIC_REQUIRE(Borrowable<std::vector<int> &>);
    STATIC_REQUIRE(Borrowable<const std::vector<int> &>);
    STATIC_REQUIRE_FALSE(Borrowable<std::vector<int>>);
    STATIC_REQUIRE_FALSE(Borrowable<const std::vector<int>>);
}

TEST_CASE("Cow::owned holds the value", "[containers][Cow]") {
    const auto cow = Cow<std::vector<int>>::owned({1, 2, 3});

    CHECK(cow.is_owned());
    CHECK_FALSE(cow.is_borrowed());
    CHECK(*cow == std::vector<int>{1, 2, 3});
    CHECK(cow->size() == 3);
}

TEST_CASE("Cow::owned rejects named values", "[containers][Cow]") {
    STATIC_REQUIRE(Ownable<std::vector<int>>);
    STATIC_REQUIRE_FALSE(Ownable<std::vector<int> &>);
    STATIC_REQUIRE_FALSE(Ownable<const std::vector<int> &>);
}

TEST_CASE("Cow gives read-only access through named values only", "[containers][Cow]") {
    STATIC_REQUIRE(std::is_same_v<decltype(*std::declval<Cow<std::vector<int>> &>()), const std::vector<int> &>);
    STATIC_REQUIRE(std::is_same_v<decltype(std::declval<Cow<std::vector<int>> &>().operator->()), const std::vector<int> *>);

    STATIC_REQUIRE(Dereferenceable<Cow<std::vector<int>> &>);
    STATIC_REQUIRE(Dereferenceable<const Cow<std::vector<int>> &>);
    STATIC_REQUIRE_FALSE(Dereferenceable<Cow<std::vector<int>>>);
    STATIC_REQUIRE_FALSE(Dereferenceable<const Cow<std::vector<int>>>);

    STATIC_REQUIRE(ArrowAccessible<Cow<std::vector<int>> &>);
    STATIC_REQUIRE(ArrowAccessible<const Cow<std::vector<int>> &>);
    STATIC_REQUIRE_FALSE(ArrowAccessible<Cow<std::vector<int>>>);
    STATIC_REQUIRE_FALSE(ArrowAccessible<const Cow<std::vector<int>>>);
}

TEST_CASE("Cow converts to a const reference only when named", "[containers][Cow]") {
    STATIC_REQUIRE(std::is_convertible_v<Cow<std::vector<int>> &, const std::vector<int> &>);
    STATIC_REQUIRE(std::is_convertible_v<const Cow<std::vector<int>> &, const std::vector<int> &>);
    STATIC_REQUIRE_FALSE(std::is_convertible_v<Cow<std::vector<int>>, const std::vector<int> &>);
    STATIC_REQUIRE_FALSE(std::is_convertible_v<const Cow<std::vector<int>>, const std::vector<int> &>);

    const std::vector<int> source = {1, 2, 3};
    const auto cow = Cow<std::vector<int>>::borrowed(source);
    const std::vector<int> &reference = cow;
    CHECK(&reference == &source);
}

TEST_CASE("Cow::visit calls the visitor for the current case", "[containers][Cow]") {
    SECTION("owned") {
        auto cow = Cow<std::vector<int>>::owned({1, 2, 3});

        cow.visit(
            [](Cow<std::vector<int>>::Owned &owned) {
                owned.value.push_back(4);
            },
            [](const Cow<std::vector<int>>::Borrowed &) {
                FAIL("visited the borrowed case");
            });

        CHECK(*cow == std::vector<int>{1, 2, 3, 4});
    }

    SECTION("borrowed") {
        const std::vector<int> source = {1, 2, 3};
        const auto cow = Cow<std::vector<int>>::borrowed(source);

        const std::vector<int> *visited = cow.visit(
            [](const Cow<std::vector<int>>::Owned &) -> const std::vector<int> * {
                FAIL("visited the owned case");
                return nullptr;
            },
            [](const Cow<std::vector<int>>::Borrowed &borrowed) -> const std::vector<int> * {
                return &borrowed.value.get();
            });

        CHECK(visited == &source);
    }
}

TEST_CASE("Cow::visit rejects a const temporary", "[containers][Cow]") {
    STATIC_REQUIRE(Visitable<Cow<std::vector<int>> &>);
    STATIC_REQUIRE(Visitable<const Cow<std::vector<int>> &>);
    STATIC_REQUIRE(Visitable<Cow<std::vector<int>>>);
    STATIC_REQUIRE_FALSE(Visitable<const Cow<std::vector<int>>>);
}

TEST_CASE("Cow::to_mut copies a borrowed value and leaves the source unchanged", "[containers][Cow]") {
    const std::vector<int> source = {1, 2, 3};
    auto cow = Cow<std::vector<int>>::borrowed(source);

    cow.to_mut().push_back(4);

    CHECK(cow.is_owned());
    CHECK(*cow == std::vector<int>{1, 2, 3, 4});
    CHECK(source == std::vector<int>{1, 2, 3});
}

TEST_CASE("Cow::to_mut writes an owned value in place", "[containers][Cow]") {
    uint32_t copies = 0;
    auto cow = Cow<CopyCounter>::owned(CopyCounter(copies));
    const CopyCounter *before = &*cow;

    CHECK(&cow.to_mut() == before);
    CHECK(cow.is_owned());
    CHECK(copies == 0);
}

TEST_CASE("Cow::to_mut rejects a temporary Cow", "[containers][Cow]") {
    STATIC_REQUIRE(MutablyAccessible<Cow<std::vector<int>> &>);
    STATIC_REQUIRE_FALSE(MutablyAccessible<Cow<std::vector<int>>>);
}

TEST_CASE("Cow::into_owned moves an owned value out", "[containers][Cow]") {
    uint32_t copies = 0;
    auto cow = Cow<CopyCounter>::owned(CopyCounter(copies));

    const CopyCounter result = std::move(cow).into_owned();

    CHECK(result.copies == &copies);
    CHECK(copies == 0);
}

TEST_CASE("Cow::into_owned copies a borrowed value", "[containers][Cow]") {
    uint32_t copies = 0;
    const CopyCounter source(copies);
    auto cow = Cow<CopyCounter>::borrowed(source);

    const CopyCounter result = std::move(cow).into_owned();

    CHECK(&result != &source);
    CHECK(copies == 1);
}

TEST_CASE("Cow::into_owned rejects a Cow that is not moved", "[containers][Cow]") {
    STATIC_REQUIRE(IntoOwnable<Cow<std::vector<int>>>);
    STATIC_REQUIRE_FALSE(IntoOwnable<Cow<std::vector<int>> &>);
}

TEST_CASE("Cow copies keep the borrow and own their value", "[containers][Cow]") {
    SECTION("borrowed") {
        const std::vector<int> source = {1, 2};
        const auto cow = Cow<std::vector<int>>::borrowed(source);

        const Cow<std::vector<int>> copy = cow;

        CHECK(copy.is_borrowed());
        CHECK(&*copy == &source);
    }

    SECTION("owned") {
        const auto cow = Cow<std::vector<int>>::owned({1, 2, 3});

        auto copy = cow;
        copy.to_mut().push_back(4);

        CHECK(*cow == std::vector<int>{1, 2, 3});
        CHECK(*copy == std::vector<int>{1, 2, 3, 4});
    }
}