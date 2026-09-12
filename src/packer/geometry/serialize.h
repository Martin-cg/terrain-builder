#pragma once

#include <cstdint>
#include <span>

#include <tl/expected.hpp>

#include "PageError.h"
#include "codec.h"
#include "format.h"
#include "io/envelope.h"
#include "try.h"

namespace packer::geometry {

// The page held by the envelope `bytes`.
[[nodiscard]] inline tl::expected<Page, PageError> read(const std::span<const uint8_t> bytes) {
    TRY(const EncodedPage encoded, io::envelope::deserialize<EncodedPageSchema>(bytes));
    return decode(encoded);
}

}