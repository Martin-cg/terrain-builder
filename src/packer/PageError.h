#pragma once

#include <compare>
#include <variant>

#include "io/compression.h"
#include "meshopt/encode.h"

namespace packer {

// A page whose streams fail to decode or contradict each other.
struct PageMalformed {
    auto operator<=>(const PageMalformed &) const = default;
};

using PageError = std::variant<io::envelope::Error, meshopt::DecodeError, PageMalformed>;

}