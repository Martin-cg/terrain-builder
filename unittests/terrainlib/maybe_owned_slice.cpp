#include "../catch2_helpers.h"
#include "containers/MaybeOwnedSlice.h"

#include <span>
#include <utility>
#include <vector>

TEST_CASE("MaybeOwnedSlice holds the values it is given ownership of", "[containers][maybe_owned_slice]") {
    std::vector<int> values = {1, 2, 3};
    const MaybeOwnedSlice<int> slice(std::move(values));

    CHECK(slice.is_owned());
    CHECK_FALSE(slice.is_ref());
    CHECK(slice.get().size() == 3);
    CHECK(slice.get()[0] == 1);
    CHECK(slice.get()[2] == 3);
}

TEST_CASE("MaybeOwnedSlice reads a borrowed range through the original", "[containers][maybe_owned_slice]") {
    std::vector<int> original = {10, 20, 30};
    const MaybeOwnedSlice<int> slice{std::span<const int>(original)};

    CHECK(slice.is_ref());
    CHECK_FALSE(slice.is_owned());
    CHECK(slice.get().data() == original.data());

    original[1] = 21;
    CHECK(slice.get()[1] == 21);
}

TEST_CASE("MaybeOwnedSlice::from_owned and from_ref make the same slices as the constructors", "[containers][maybe_owned_slice]") {
    const std::vector<int> original = {4, 5};

    const MaybeOwnedSlice<int> owned = MaybeOwnedSlice<int>::from_owned({4, 5});
    const MaybeOwnedSlice<int> borrowed = MaybeOwnedSlice<int>::from_ref(original);

    CHECK(owned.is_owned());
    CHECK(borrowed.is_ref());
    CHECK(borrowed.get().data() == original.data());
}

TEST_CASE("MaybeOwnedSlice keeps its values where they are when it is moved", "[containers][maybe_owned_slice]") {
    std::vector<int> original = {7, 8, 9};
    const int *const borrowed_data = original.data();

    MaybeOwnedSlice<int> owned(std::vector<int>{7, 8, 9});
    const int *const owned_data = owned.get().data();
    const MaybeOwnedSlice<int> moved_owned = std::move(owned);

    MaybeOwnedSlice<int> borrowed{std::span<const int>(original)};
    const MaybeOwnedSlice<int> moved_borrowed = std::move(borrowed);

    CHECK(moved_owned.get().data() == owned_data);
    CHECK(moved_borrowed.get().data() == borrowed_data);
    CHECK(moved_owned == moved_borrowed);
}

TEST_CASE("MaybeOwnedSlice copies the values it owns", "[containers][maybe_owned_slice]") {
    const MaybeOwnedSlice<int> owned(std::vector<int>{1, 2});
    const MaybeOwnedSlice<int> copy = owned;

    CHECK(copy.get().data() != owned.get().data());
    CHECK(copy == owned);
}

TEST_CASE("MaybeOwnedSlice compares its values, not how they are held", "[containers][maybe_owned_slice]") {
    const std::vector<int> original = {1, 2, 3};

    const MaybeOwnedSlice<int> owned(std::vector<int>{1, 2, 3});
    const MaybeOwnedSlice<int> borrowed = MaybeOwnedSlice<int>::from_ref(original);
    const MaybeOwnedSlice<int> shorter(std::vector<int>{1, 2});
    const MaybeOwnedSlice<int> different(std::vector<int>{1, 2, 4});

    CHECK(owned == borrowed);
    CHECK(owned != shorter);
    CHECK(owned != different);
}
