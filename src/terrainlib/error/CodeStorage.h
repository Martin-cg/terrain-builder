#pragma once

#include "error/ErasedCode.h"
#include "error/TypedCode.h"

namespace alp {

namespace detail {

template <typename... Es>
struct SelectCodeStorage {
    using type = TypedCode<Es...>;
};

template <>
struct SelectCodeStorage<> {
    using type = ErasedCode;
};

} // namespace detail

// How an error with the codes Es stores its code: a TypedCode, or an ErasedCode when Es is empty.
template <typename... Es>
using CodeStorage = typename detail::SelectCodeStorage<Es...>::type;

} // namespace alp
