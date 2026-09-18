#pragma once

#include <fmt/format.h>

#include "error/format.h"
#include "log.h"

// Logs `error` as a report of its causes and its trace, with the message formatted from `...`, if any, as its outermost context.
#define LOG_FAILURE(error, ...) LOG_ERROR("{:?}", auto(error) __VA_OPT__(.context(::fmt::format(__VA_ARGS__))))