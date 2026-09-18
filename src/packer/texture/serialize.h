#pragma once

#include <cstdint>
#include <span>
#include <utility>
#include <variant>
#include <vector>

#include <opencv2/core.hpp>
#include <tl/expected.hpp>

#include "basis.h"
#include "containers/MaybeOwnedSlice.h"
#include "format.h"
#include "io/envelope.h"
#include "try.h"

namespace packer::texture {

// Why a texture page could not be written.
using SerializeError = std::variant<io::envelope::Error, basis::EncodeError>;

// Why a texture page could not be read.
using DeserializeError = std::variant<io::envelope::Error, basis::DecodeError>;

// Encodes `textures` into an encoded texture page.
[[nodiscard]] inline tl::expected<std::vector<uint8_t>, SerializeError> serialize(const std::span<const cv::Mat> textures, const basis::EncodeOptions &options) {
    TRY(std::vector<uint8_t> basis_file, basis::encode(textures, options));
    // Basis output does not compress any further, so the envelope only checksums it.
    return io::envelope::serialize<EncodedPageSchema, EncodedPageSchema::latest_version>(
        EncodedPage{.basis_file = std::move(basis_file)},
        io::envelope::CompressionAlgorithm::None,
        io::envelope::ChecksumAlgorithm::Crc32c);
}

// Opens a reader over the page in `bytes`.
[[nodiscard]] inline tl::expected<basis::Reader, DeserializeError> deserialize(const std::span<const uint8_t> bytes) {
    TRY(EncodedPage encoded, io::envelope::deserialize<EncodedPageSchema>(bytes));
    TRY(basis::Reader reader, basis::Reader::open(MaybeOwnedSlice<uint8_t>::from_owned(std::move(encoded.basis_file))));
    return reader;
}

}
