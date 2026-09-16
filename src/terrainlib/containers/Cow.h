#pragma once

#include <concepts>
#include <functional>
#include <type_traits>
#include <utility>
#include <variant>

#include "variant_utils.h"

template <typename T>
concept NotPtrOrRef = (!std::is_pointer_v<T>) && (!std::is_reference_v<T>);

// A value that is either borrowed read-only or owned, copied only when mutable access or ownership is requested.
template <NotPtrOrRef T>
    requires(!std::is_const_v<T> && std::copy_constructible<T>)
class Cow {
public:
    // The case holding its own value.
    struct Owned {
        T value;
    };
    // The case referring to a value that lives elsewhere.
    struct Borrowed {
        std::reference_wrapper<const T> value;
    };

    // Borrows `value`, which must outlive the Cow.
    static Cow borrowed(const T &value) {
        return Cow(Borrowed{value});
    }
    static Cow borrowed(const T &&value) = delete;
    static Cow owned(T &&value) {
        return Cow(Owned{std::move(value)});
    }

    bool is_borrowed() const noexcept {
        return std::holds_alternative<Borrowed>(this->_data);
    }
    bool is_owned() const noexcept {
        return std::holds_alternative<Owned>(this->_data);
    }

    // Calls whichever of `visitors` takes the current case, as `Owned` or `Borrowed`.
    template <typename... Visitors>
    decltype(auto) visit(Visitors &&...visitors) & {
        return match(this->_data, std::forward<Visitors>(visitors)...);
    }
    template <typename... Visitors>
    decltype(auto) visit(Visitors &&...visitors) const & {
        return match(this->_data, std::forward<Visitors>(visitors)...);
    }
    template <typename... Visitors>
    decltype(auto) visit(Visitors &&...visitors) && {
        return match(std::move(this->_data), std::forward<Visitors>(visitors)...);
    }
    template <typename... Visitors>
    decltype(auto) visit(Visitors &&...visitors) const && = delete;

    const T &operator*() const & {
        return this->visit(
            [](const Owned &owned) -> const T & {
                return owned.value;
            },
            [](const Borrowed &borrowed) -> const T & {
                return borrowed.value.get();
            });
    }
    const T &operator*() const && = delete;

    const T *operator->() const & {
        return &**this;
    }
    const T *operator->() const && = delete;

    operator const T &() const & {
        return **this;
    }
    operator const T &() const && = delete;

    // Gives mutable access to the owned value, copying the borrowed one into the Cow first.
    T &to_mut() & {
        return this->visit(
            [](Owned &owned) -> T & {
                return owned.value;
            },
            [this](const Borrowed &borrowed) -> T & {
                // Emplacing destroys `borrowed`, so the source it refers to is bound first.
                const T &source = borrowed.value.get();
                return this->_data.template emplace<Owned>(source).value;
            });
    }
    // Moves the owned value out, or copies the borrowed one.
    T into_owned() && {
        return std::move(*this).visit(
            [](Owned &&owned) -> T {
                return std::move(owned.value);
            },
            [](const Borrowed &borrowed) -> T {
                return borrowed.value.get();
            });
    }

private:
    explicit Cow(std::variant<Owned, Borrowed> data) : _data(std::move(data)) {}

    std::variant<Owned, Borrowed> _data;
};
