#pragma once

#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <fmt/format.h>
#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>

#include "error/Error.h"
#include "error/Result.h"
#include "io/FileError.h"
#include "io/UnsupportedFormat.h"

namespace texture::io {

// Bytes that are not an image OpenCV can decode.
struct DecodeError {
    bool operator==(const DecodeError &) const = default;
};

inline std::string_view format_as(const DecodeError) {
    return "the bytes are not an image that can be decoded";
}

// An image OpenCV failed to encode, with the reason it gave.
struct EncodeError {
    std::string reason;

    bool operator==(const EncodeError &) const = default;
};

} // namespace texture::io

template <>
struct fmt::formatter<texture::io::EncodeError> {
    constexpr fmt::format_parse_context::iterator parse(fmt::format_parse_context &context) {
        return context.begin();
    }

    fmt::format_context::iterator format(const texture::io::EncodeError &error, fmt::format_context &context) const {
        return fmt::format_to(context.out(), "the image could not be encoded: {}", error.reason);
    }
};

namespace texture::io {

// An error from reading an image from a file.
using ReadError = alp::Error<::io::FileError, DecodeError>;

// An error from writing an image to a file.
using WriteError = alp::Error<::io::FileError, ::io::UnsupportedFormat, EncodeError>;

// Decodes an encoded image, converting it as `mode` asks.
[[nodiscard]]
alp::Result<cv::Mat, DecodeError> decode_bytes(std::span<const uint8_t> bytes, cv::ImreadModes mode = cv::IMREAD_COLOR);

// Encodes `image` in the format that `extension_with_dot` names.
[[nodiscard]]
alp::Result<std::vector<uint8_t>, ::io::UnsupportedFormat, EncodeError> encode_to_bytes(const cv::Mat &image, std::string_view extension_with_dot);

// Reads and decodes the image file at `path`, converting it as `mode` asks.
[[nodiscard]]
alp::Result<cv::Mat, ReadError> read_from_path(const std::filesystem::path &path, cv::ImreadModes mode = cv::IMREAD_COLOR);

// Encodes `image` in the format that the extension of `path` names and writes it to the file at `path`.
[[nodiscard]]
alp::Result<void, WriteError> write_to_path(const cv::Mat &image, const std::filesystem::path &path);

} // namespace texture::io
