#pragma once

#include <cstdint>
#include <span>
#include <utility>
#include <vector>

#include <tl/expected.hpp>

#include "format.h"
#include "io/envelope.h"
#include "reader.h"
#include "try.h"

namespace packer::hierarchy {

// Why a hierarchy page could not be written.
using SerializeError = io::envelope::Error;

// Why a hierarchy page could not be read.
using DeserializeError = io::envelope::Error;

// Serializes `page` into bytes.
[[nodiscard]] inline tl::expected<std::vector<uint8_t>, SerializeError> serialize(const EncodedPage &page) {
    return io::envelope::serialize<EncodedPageSchema, EncodedPageSchema::latest_version>(page);
}

// Opens a reader over the hierarchy page in `bytes`.
[[nodiscard]] inline tl::expected<PageReader, DeserializeError> deserialize(const std::span<const uint8_t> bytes) {
    TRY(EncodedPage encoded, io::envelope::deserialize<EncodedPageSchema>(bytes));
    return PageReader(std::move(encoded));
}

}
