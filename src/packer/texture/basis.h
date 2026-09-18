#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

#include <glm/glm.hpp>
#include <opencv2/core.hpp>
#include <tl/expected.hpp>

#include "containers/MaybeOwnedSlice.h"

namespace packer::texture::basis {

// The format the images of a texture page are stored in.
enum class EncodeFormat : uint8_t {
    Etc1s,
    UastcLdr4x4,
    UastcHdr4x4,
    AstcHdr6x6,
    UastcHdr6x6Intermediate,
    XuastcLdr4x4,
    XuastcLdr5x4,
    XuastcLdr5x5,
    XuastcLdr6x5,
    XuastcLdr6x6,
    XuastcLdr8x5,
    XuastcLdr8x6,
    XuastcLdr10x5,
    XuastcLdr10x6,
    XuastcLdr8x8,
    XuastcLdr10x8,
    XuastcLdr10x10,
    XuastcLdr12x10,
    XuastcLdr12x12,
    AstcLdr4x4,
    AstcLdr5x4,
    AstcLdr5x5,
    AstcLdr6x5,
    AstcLdr6x6,
    AstcLdr8x5,
    AstcLdr8x6,
    AstcLdr10x5,
    AstcLdr10x6,
    AstcLdr8x8,
    AstcLdr10x8,
    AstcLdr10x10,
    AstcLdr12x10,
    AstcLdr12x12,
    Xubc7,
};

// The format an image of a texture page is transcoded to.
enum class TranscodeFormat : uint8_t {
    Etc1Rgb,
    Etc2Rgba,
    Bc1Rgb,
    Bc3Rgba,
    Bc4R,
    Bc5Rg,
    Bc7Rgba,
    Pvrtc1Rgb,
    Pvrtc1Rgba,
    AstcLdr4x4Rgba,
    AtcRgb,
    AtcRgba,
    Rgba32,
    Rgb565,
    Bgr565,
    Rgba4444,
    Fxt1Rgb,
    Pvrtc2Rgb,
    Pvrtc2Rgba,
    Etc2EacR11,
    Etc2EacRg11,
    Bc6h,
    AstcHdr4x4Rgba,
    RgbHalf,
    RgbaHalf,
    Rgb9e5,
    AstcHdr6x6Rgba,
    AstcLdr5x4Rgba,
    AstcLdr5x5Rgba,
    AstcLdr6x5Rgba,
    AstcLdr6x6Rgba,
    AstcLdr8x5Rgba,
    AstcLdr8x6Rgba,
    AstcLdr10x5Rgba,
    AstcLdr10x6Rgba,
    AstcLdr8x8Rgba,
    AstcLdr10x8Rgba,
    AstcLdr10x10Rgba,
    AstcLdr12x10Rgba,
    AstcLdr12x12Rgba,
};

// How the images of a texture page are encoded.
struct EncodeOptions {
    EncodeFormat format = EncodeFormat::XuastcLdr6x6;
    // The trade-off between quality and size, from 1 to 100.
    uint32_t quality = 90;
    // How much of the search space the encoder covers, from 0 to 10.
    uint32_t effort = 3;
    uint32_t thread_count = 1;

    auto operator<=>(const EncodeOptions &) const = default;
};

// Why textures could not be encoded.
struct EncodeError {
    enum class Kind : uint8_t {
        InvalidOptions,
        InvalidTextures,
        EncodingFailed,
    };

    Kind kind;
    // The value Basis Universal reported, as its basis_compressor::error_code names it.
    int code;

    auto operator<=>(const EncodeError &) const = default;
};

// Why a texture page could not be read.
enum class DecodeError : uint8_t {
    TranscodingFailed,
    UnsupportedFormat,
};

// Encodes `textures` as the images of one texture page, in order.
[[nodiscard]] tl::expected<std::vector<uint8_t>, EncodeError> encode(std::span<const cv::Mat> textures, const EncodeOptions &options);

// Encodes `texture` as the only image of one texture page.
[[nodiscard]] tl::expected<std::vector<uint8_t>, EncodeError> encode(const cv::Mat &texture, const EncodeOptions &options);

// Reads the images of one texture page.
class Reader {
public:
    // Opens `basis_file`, or fails if it is not a texture page this version can read.
    [[nodiscard]] static tl::expected<Reader, DecodeError> open(MaybeOwnedSlice<uint8_t> basis_file);

    // The number of images in the page.
    [[nodiscard]] size_t size() const;

    // The size of the image at `index`, in pixels.
    [[nodiscard]] glm::uvec2 get_image_size(const size_t index) const;

    // The image at `index`, transcoded to 8-bit BGR.
    [[nodiscard]] tl::expected<cv::Mat, DecodeError> decode_image(const size_t index) const;

    // The number of bytes the image at `index` takes in `format`.
    [[nodiscard]] size_t get_transcoded_size(const size_t index, const TranscodeFormat format) const;

    // Transcodes the image at `index` into `buffer`, which holds `get_transcoded_size(index, format)` bytes.
    [[nodiscard]] tl::expected<void, DecodeError> transcode_image(const size_t index, const TranscodeFormat format, std::span<uint8_t> buffer) const;

private:
    struct Impl;
    struct ImplDeleter {
        void operator()(Impl *impl) const;
    };

    Reader(MaybeOwnedSlice<uint8_t> basis_file, std::unique_ptr<Impl, ImplDeleter> impl);

    MaybeOwnedSlice<uint8_t> _basis_file;
    std::unique_ptr<Impl, ImplDeleter> _impl;
};

}
