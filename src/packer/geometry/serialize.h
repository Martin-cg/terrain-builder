#pragma once

#include <cstdint>
#include <span>
#include <variant>
#include <vector>

#include <tl/expected.hpp>

#include "codec.h"
#include "format.h"
#include "io/envelope.h"
#include "meshopt/encode.h"
#include "try.h"

namespace packer::geometry {

// Why a geometry page could not be written.
using SerializeError = io::envelope::Error;

// Why a geometry page could not be read.
using DeserializeError = std::variant<io::envelope::Error, meshopt::DecodeError>;

// Serializes `page` into bytes.
[[nodiscard]] inline tl::expected<std::vector<uint8_t>, SerializeError> serialize(const Page &page) {
    return io::envelope::serialize<EncodedPageSchema, EncodedPageSchema::latest_version>(encode(page));
}

// Deserializes the geometry page in `bytes`.
[[nodiscard]] inline tl::expected<Page, DeserializeError> deserialize(const std::span<const uint8_t> bytes) {
    TRY(const EncodedPage encoded, io::envelope::deserialize<EncodedPageSchema>(bytes));
    return decode(encoded);
}

}