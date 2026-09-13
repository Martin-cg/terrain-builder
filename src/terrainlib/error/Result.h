#pragma once

#include <tl/expected.hpp>

#include "error/Error.h"
#include "error/ErrorCode.h"

namespace alp {

// A value, or an error with a code of type Es; with no Es, an erased error.
template <typename T, ErrorCode... Es>
using Result = tl::expected<T, Error<Es...>>;

} // namespace alp
