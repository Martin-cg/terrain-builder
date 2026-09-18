#pragma once

#include <compare>
#include <cstdint>
#include <vector>

#include "io/envelope.h"

namespace packer::texture {

namespace v1 {

// A texture page's payload.
struct EncodedPage {
    // The .basis file the page's images were encoded into.
    std::vector<uint8_t> basis_file;

    auto operator<=>(const EncodedPage &) const = default;
};

}

using EncodedPage = v1::EncodedPage;
using EncodedPageSchema = io::envelope::PayloadSchema<"TexturePage", io::envelope::Version<1, v1::EncodedPage>>;

}
