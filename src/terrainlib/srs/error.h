#pragma once

#include <string>

#include <fmt/format.h>

#include "error/Error.h"

namespace srs {

// No transformation exists from one spatial reference to another.
struct MissingTransformation {
    std::string source;
    std::string target;

    bool operator==(const MissingTransformation &) const = default;
};

inline std::string format_as(const MissingTransformation &error) {
    return fmt::format("no transformation exists from {} to {}", error.source, error.target);
}

// A transformation from one spatial reference to another cannot map at least one point.
struct UntransformablePoint {
    std::string source;
    std::string target;

    bool operator==(const UntransformablePoint &) const = default;
};

inline std::string format_as(const UntransformablePoint &error) {
    return fmt::format("a point cannot be transformed from {} to {}", error.source, error.target);
}

// An error from transforming between two spatial references.
using TransformError = alp::Error<MissingTransformation, UntransformablePoint>;

}
