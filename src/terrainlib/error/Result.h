#pragma once

#include <expected>

#include "error/Error.h"
#include "error/ErrorCode.h"

namespace alp {

// The underlying result type; a value, or an error of type E.
template <typename T, typename E>
using BasicResult = std::expected<T, E>;

// A value, or an error with codes of type Es; with no Es, an erased error.
template <typename T, ErrorCodes... Es>
using Result = BasicResult<T, Error<Es...>>;

} // namespace alp
