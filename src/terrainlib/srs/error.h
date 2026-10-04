#pragma once

#include <optional>
#include <string>

#include <fmt/format.h>

#include "error/Error.h"
#include "gdal/CplError.h"

namespace srs {

// No transformation exists from one spatial reference to another.
struct MissingTransformation {
    std::string source;
    std::string target;
    // The error GDAL recorded for the failure, if it recorded one.
    std::optional<gdal::CplError> gdal_error;

    bool operator==(const MissingTransformation &) const = default;
};

inline std::string format_as(const MissingTransformation &error) {
    if (error.gdal_error.has_value()) {
        return fmt::format("no transformation exists from {} to {}: {}", error.source, error.target, error.gdal_error->message());
    }
    return fmt::format("no transformation exists from {} to {}", error.source, error.target);
}

// A transformation from one spatial reference to another cannot map at least one point.
struct UntransformablePoint {
    std::string source;
    std::string target;
    // The error GDAL recorded for the failure, if it recorded one. (skipped after 20 errors)
    std::optional<gdal::CplError> gdal_error;

    bool operator==(const UntransformablePoint &) const = default;
};

inline std::string format_as(const UntransformablePoint &error) {
    if (error.gdal_error.has_value()) {
        return fmt::format("a point cannot be transformed from {} to {}: {}", error.source, error.target, error.gdal_error->message());
    }
    return fmt::format("a point cannot be transformed from {} to {}", error.source, error.target);
}

// An error from transforming between two spatial references.
using TransformError = alp::Error<MissingTransformation, UntransformablePoint>;

}
