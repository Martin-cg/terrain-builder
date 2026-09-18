#include "texture/basis.h"

#include <algorithm>
#include <mutex>
#include <utility>

#include <encoder/basisu_comp.h>
#include <libassert/assert.hpp>
#include <magic_enum/magic_enum.hpp>
#include <opencv2/imgproc.hpp>
#include <transcoder/basisu_transcoder.h>

#include "log.h"
#include "numeric/checked_cast.h"
#include "opencv_utils.h"
#include "try.h"

namespace packer::texture::basis {

namespace {

// Basis Universal keeps its lookup tables in globals, so they are built once for the process.
void init_basis() {
    static std::once_flag initialized;
    std::call_once(initialized, [] {
        basisu::basisu_encoder_init();
        basist::basisu_transcoder_init();
    });
}

basist::basis_tex_format to_basis(const EncodeFormat format) {
    switch (format) {
    case EncodeFormat::Etc1s:
        return basist::basis_tex_format::cETC1S;
    case EncodeFormat::UastcLdr4x4:
        return basist::basis_tex_format::cUASTC_LDR_4x4;
    case EncodeFormat::UastcHdr4x4:
        return basist::basis_tex_format::cUASTC_HDR_4x4;
    case EncodeFormat::AstcHdr6x6:
        return basist::basis_tex_format::cASTC_HDR_6x6;
    case EncodeFormat::UastcHdr6x6Intermediate:
        return basist::basis_tex_format::cUASTC_HDR_6x6_INTERMEDIATE;
    case EncodeFormat::XuastcLdr4x4:
        return basist::basis_tex_format::cXUASTC_LDR_4x4;
    case EncodeFormat::XuastcLdr5x4:
        return basist::basis_tex_format::cXUASTC_LDR_5x4;
    case EncodeFormat::XuastcLdr5x5:
        return basist::basis_tex_format::cXUASTC_LDR_5x5;
    case EncodeFormat::XuastcLdr6x5:
        return basist::basis_tex_format::cXUASTC_LDR_6x5;
    case EncodeFormat::XuastcLdr6x6:
        return basist::basis_tex_format::cXUASTC_LDR_6x6;
    case EncodeFormat::XuastcLdr8x5:
        return basist::basis_tex_format::cXUASTC_LDR_8x5;
    case EncodeFormat::XuastcLdr8x6:
        return basist::basis_tex_format::cXUASTC_LDR_8x6;
    case EncodeFormat::XuastcLdr10x5:
        return basist::basis_tex_format::cXUASTC_LDR_10x5;
    case EncodeFormat::XuastcLdr10x6:
        return basist::basis_tex_format::cXUASTC_LDR_10x6;
    case EncodeFormat::XuastcLdr8x8:
        return basist::basis_tex_format::cXUASTC_LDR_8x8;
    case EncodeFormat::XuastcLdr10x8:
        return basist::basis_tex_format::cXUASTC_LDR_10x8;
    case EncodeFormat::XuastcLdr10x10:
        return basist::basis_tex_format::cXUASTC_LDR_10x10;
    case EncodeFormat::XuastcLdr12x10:
        return basist::basis_tex_format::cXUASTC_LDR_12x10;
    case EncodeFormat::XuastcLdr12x12:
        return basist::basis_tex_format::cXUASTC_LDR_12x12;
    case EncodeFormat::AstcLdr4x4:
        return basist::basis_tex_format::cASTC_LDR_4x4;
    case EncodeFormat::AstcLdr5x4:
        return basist::basis_tex_format::cASTC_LDR_5x4;
    case EncodeFormat::AstcLdr5x5:
        return basist::basis_tex_format::cASTC_LDR_5x5;
    case EncodeFormat::AstcLdr6x5:
        return basist::basis_tex_format::cASTC_LDR_6x5;
    case EncodeFormat::AstcLdr6x6:
        return basist::basis_tex_format::cASTC_LDR_6x6;
    case EncodeFormat::AstcLdr8x5:
        return basist::basis_tex_format::cASTC_LDR_8x5;
    case EncodeFormat::AstcLdr8x6:
        return basist::basis_tex_format::cASTC_LDR_8x6;
    case EncodeFormat::AstcLdr10x5:
        return basist::basis_tex_format::cASTC_LDR_10x5;
    case EncodeFormat::AstcLdr10x6:
        return basist::basis_tex_format::cASTC_LDR_10x6;
    case EncodeFormat::AstcLdr8x8:
        return basist::basis_tex_format::cASTC_LDR_8x8;
    case EncodeFormat::AstcLdr10x8:
        return basist::basis_tex_format::cASTC_LDR_10x8;
    case EncodeFormat::AstcLdr10x10:
        return basist::basis_tex_format::cASTC_LDR_10x10;
    case EncodeFormat::AstcLdr12x10:
        return basist::basis_tex_format::cASTC_LDR_12x10;
    case EncodeFormat::AstcLdr12x12:
        return basist::basis_tex_format::cASTC_LDR_12x12;
    case EncodeFormat::Xubc7:
        return basist::basis_tex_format::cXUBC7;
    }
    UNREACHABLE("unknown encode format");
}

basist::transcoder_texture_format to_basis(const TranscodeFormat format) {
    switch (format) {
    case TranscodeFormat::Etc1Rgb:
        return basist::transcoder_texture_format::cTFETC1_RGB;
    case TranscodeFormat::Etc2Rgba:
        return basist::transcoder_texture_format::cTFETC2_RGBA;
    case TranscodeFormat::Bc1Rgb:
        return basist::transcoder_texture_format::cTFBC1_RGB;
    case TranscodeFormat::Bc3Rgba:
        return basist::transcoder_texture_format::cTFBC3_RGBA;
    case TranscodeFormat::Bc4R:
        return basist::transcoder_texture_format::cTFBC4_R;
    case TranscodeFormat::Bc5Rg:
        return basist::transcoder_texture_format::cTFBC5_RG;
    case TranscodeFormat::Bc7Rgba:
        return basist::transcoder_texture_format::cTFBC7_RGBA;
    case TranscodeFormat::Pvrtc1Rgb:
        return basist::transcoder_texture_format::cTFPVRTC1_4_RGB;
    case TranscodeFormat::Pvrtc1Rgba:
        return basist::transcoder_texture_format::cTFPVRTC1_4_RGBA;
    case TranscodeFormat::AstcLdr4x4Rgba:
        return basist::transcoder_texture_format::cTFASTC_LDR_4x4_RGBA;
    case TranscodeFormat::AtcRgb:
        return basist::transcoder_texture_format::cTFATC_RGB;
    case TranscodeFormat::AtcRgba:
        return basist::transcoder_texture_format::cTFATC_RGBA;
    case TranscodeFormat::Rgba32:
        return basist::transcoder_texture_format::cTFRGBA32;
    case TranscodeFormat::Rgb565:
        return basist::transcoder_texture_format::cTFRGB565;
    case TranscodeFormat::Bgr565:
        return basist::transcoder_texture_format::cTFBGR565;
    case TranscodeFormat::Rgba4444:
        return basist::transcoder_texture_format::cTFRGBA4444;
    case TranscodeFormat::Fxt1Rgb:
        return basist::transcoder_texture_format::cTFFXT1_RGB;
    case TranscodeFormat::Pvrtc2Rgb:
        return basist::transcoder_texture_format::cTFPVRTC2_4_RGB;
    case TranscodeFormat::Pvrtc2Rgba:
        return basist::transcoder_texture_format::cTFPVRTC2_4_RGBA;
    case TranscodeFormat::Etc2EacR11:
        return basist::transcoder_texture_format::cTFETC2_EAC_R11;
    case TranscodeFormat::Etc2EacRg11:
        return basist::transcoder_texture_format::cTFETC2_EAC_RG11;
    case TranscodeFormat::Bc6h:
        return basist::transcoder_texture_format::cTFBC6H;
    case TranscodeFormat::AstcHdr4x4Rgba:
        return basist::transcoder_texture_format::cTFASTC_HDR_4x4_RGBA;
    case TranscodeFormat::RgbHalf:
        return basist::transcoder_texture_format::cTFRGB_HALF;
    case TranscodeFormat::RgbaHalf:
        return basist::transcoder_texture_format::cTFRGBA_HALF;
    case TranscodeFormat::Rgb9e5:
        return basist::transcoder_texture_format::cTFRGB_9E5;
    case TranscodeFormat::AstcHdr6x6Rgba:
        return basist::transcoder_texture_format::cTFASTC_HDR_6x6_RGBA;
    case TranscodeFormat::AstcLdr5x4Rgba:
        return basist::transcoder_texture_format::cTFASTC_LDR_5x4_RGBA;
    case TranscodeFormat::AstcLdr5x5Rgba:
        return basist::transcoder_texture_format::cTFASTC_LDR_5x5_RGBA;
    case TranscodeFormat::AstcLdr6x5Rgba:
        return basist::transcoder_texture_format::cTFASTC_LDR_6x5_RGBA;
    case TranscodeFormat::AstcLdr6x6Rgba:
        return basist::transcoder_texture_format::cTFASTC_LDR_6x6_RGBA;
    case TranscodeFormat::AstcLdr8x5Rgba:
        return basist::transcoder_texture_format::cTFASTC_LDR_8x5_RGBA;
    case TranscodeFormat::AstcLdr8x6Rgba:
        return basist::transcoder_texture_format::cTFASTC_LDR_8x6_RGBA;
    case TranscodeFormat::AstcLdr10x5Rgba:
        return basist::transcoder_texture_format::cTFASTC_LDR_10x5_RGBA;
    case TranscodeFormat::AstcLdr10x6Rgba:
        return basist::transcoder_texture_format::cTFASTC_LDR_10x6_RGBA;
    case TranscodeFormat::AstcLdr8x8Rgba:
        return basist::transcoder_texture_format::cTFASTC_LDR_8x8_RGBA;
    case TranscodeFormat::AstcLdr10x8Rgba:
        return basist::transcoder_texture_format::cTFASTC_LDR_10x8_RGBA;
    case TranscodeFormat::AstcLdr10x10Rgba:
        return basist::transcoder_texture_format::cTFASTC_LDR_10x10_RGBA;
    case TranscodeFormat::AstcLdr12x10Rgba:
        return basist::transcoder_texture_format::cTFASTC_LDR_12x10_RGBA;
    case TranscodeFormat::AstcLdr12x12Rgba:
        return basist::transcoder_texture_format::cTFASTC_LDR_12x12_RGBA;
    }
    UNREACHABLE("unknown transcode format");
}

// Basis takes 8-bit RGBA pixels, while textures reach us as grey, BGR or BGRA.
tl::expected<cv::Mat, EncodeError> to_basis_pixels(const cv::Mat &texture) {
    const EncodeError rejected{.kind = EncodeError::Kind::InvalidTextures, .code = basisu::basis_compressor::cECFailedReadingSourceImages};

    if (texture.depth() != CV_8U) {
        LOG_ERROR("A texture page takes 8-bit textures");
        return tl::unexpected(rejected);
    }

    const std::optional<cv::Mat> rgba = convert_to_rgba(texture);
    if (!rgba) {
        LOG_ERROR("A texture of {} channels cannot be encoded", texture.channels());
        return tl::unexpected(rejected);
    }

    return *rgba;
}

// The codes Basis reports for input it rejects; everything else is a failure inside the encoder.
EncodeError::Kind classify(const basisu::basis_compressor::error_code code) {
    switch (code) {
    case basisu::basis_compressor::cECFailedInvalidParameters:
        return EncodeError::Kind::InvalidOptions;
    case basisu::basis_compressor::cECFailedReadingSourceImages:
    case basisu::basis_compressor::cECFailedValidating:
        return EncodeError::Kind::InvalidTextures;
    default:
        return EncodeError::Kind::EncodingFailed;
    }
}

}

tl::expected<std::vector<uint8_t>, EncodeError> encode(std::span<const cv::Mat> textures, const EncodeOptions &options) {
    init_basis();

    if (textures.empty()) {
        LOG_ERROR("A texture page needs at least one texture");
        return tl::unexpected(EncodeError{.kind = EncodeError::Kind::InvalidTextures, .code = basisu::basis_compressor::cECFailedReadingSourceImages});
    }

    // Basis clamps or reinterprets values outside these ranges instead of rejecting them.
    if (options.quality < 1 || options.quality > 100 || options.effort > 10) {
        LOG_ERROR("Quality {} or effort {} is outside its range", options.quality, options.effort);
        return tl::unexpected(EncodeError{.kind = EncodeError::Kind::InvalidOptions, .code = basisu::basis_compressor::cECFailedInvalidParameters});
    }

    basisu::job_pool job_pool(std::max(options.thread_count, 1u));

    basisu::basis_compressor_params params;
    if (!params.set_format_mode_and_quality_effort(to_basis(options.format), static_cast<int>(options.quality), static_cast<int>(options.effort))) {
        LOG_ERROR("Basis Universal rejected the encode format");
        return tl::unexpected(EncodeError{.kind = EncodeError::Kind::InvalidOptions, .code = basisu::basis_compressor::cECFailedInvalidParameters});
    }
    params.set_srgb_options(true);
    params.set_ldr_srgb_channel_weights(true);
    params.m_tex_type = basist::basis_texture_type::cBASISTexType2D;
    params.m_mip_gen = false;
    params.m_create_ktx2_file = false;
    params.m_status_output = false;
    params.m_multithreading = options.thread_count > 1;
    params.m_pJob_pool = &job_pool;

    for (const cv::Mat &texture : textures) {
        TRY(const cv::Mat rgba, to_basis_pixels(texture));
        const glm::uvec2 size = get_texture_size(rgba);
        params.m_source_images.push_back(basisu::image(rgba.data, size.x, size.y, 4));
    }

    basisu::basis_compressor compressor;
    if (!compressor.init(params)) {
        LOG_ERROR("Basis Universal rejected the encode options");
        return tl::unexpected(EncodeError{.kind = EncodeError::Kind::InvalidOptions, .code = basisu::basis_compressor::cECFailedInvalidParameters});
    }

    const basisu::basis_compressor::error_code code = compressor.process();
    if (code != basisu::basis_compressor::cECSuccess) {
        LOG_ERROR("Basis Universal failed to encode {} textures: {}", textures.size(), magic_enum::enum_name(code));
        return tl::unexpected(EncodeError{.kind = classify(code), .code = code});
    }

    const basisu::uint8_vec &file = compressor.get_output_basis_file();
    return std::vector<uint8_t>(file.begin(), file.end());
}

tl::expected<std::vector<uint8_t>, EncodeError> encode(const cv::Mat &texture, const EncodeOptions &options) {
    return encode(std::span<const cv::Mat>(&texture, 1), options);
}

struct Reader::Impl {
    basist::basisu_transcoder transcoder;
};

void Reader::ImplDeleter::operator()(Impl *impl) const {
    delete impl;
}

Reader::Reader(MaybeOwnedSlice<uint8_t> basis_file, std::unique_ptr<Impl, ImplDeleter> impl)
    : _basis_file(std::move(basis_file)), _impl(std::move(impl)) {}

tl::expected<Reader, DecodeError> Reader::open(MaybeOwnedSlice<uint8_t> basis_file) {
    init_basis();

    const std::span<const uint8_t> bytes = basis_file.get();
    const uint32_t byte_count = checked_cast<uint32_t>(bytes.size());

    std::unique_ptr<Impl, ImplDeleter> impl(new Impl());

    // For ETC1S pages this decompresses the codebooks the whole file shares, so it runs once per page.
    if (!impl->transcoder.start_transcoding(bytes.data(), byte_count)) {
        return tl::unexpected(DecodeError::TranscodingFailed);
    }

    return Reader(std::move(basis_file), std::move(impl));
}

size_t Reader::size() const {
    const std::span<const uint8_t> bytes = this->_basis_file.get();
    return this->_impl->transcoder.get_total_images(bytes.data(), static_cast<uint32_t>(bytes.size()));
}

glm::uvec2 Reader::get_image_size(const size_t index) const {
    DEBUG_ASSERT(index < this->size());

    const std::span<const uint8_t> bytes = this->_basis_file.get();
    basist::basisu_image_level_info info;
    const bool described = this->_impl->transcoder.get_image_level_info(bytes.data(), static_cast<uint32_t>(bytes.size()), info, static_cast<uint32_t>(index), 0);
    DEBUG_ASSERT(described);

    return {info.m_orig_width, info.m_orig_height};
}

tl::expected<cv::Mat, DecodeError> Reader::decode_image(const size_t index) const {
    DEBUG_ASSERT(index < this->size());

    const glm::uvec2 image_size = this->get_image_size(index);
    cv::Mat rgba(image_size.y, image_size.x, CV_8UC4);

    TRY(this->transcode_image(index, TranscodeFormat::Rgba32, std::span<uint8_t>(rgba.data, rgba.total() * rgba.elemSize())));

    cv::Mat bgr;
    cv::cvtColor(rgba, bgr, cv::COLOR_RGBA2BGR);
    return bgr;
}

size_t Reader::get_transcoded_size(const size_t index, const TranscodeFormat format) const {
    DEBUG_ASSERT(index < this->size());

    const glm::uvec2 image_size = this->get_image_size(index);
    return basist::basis_compute_transcoded_image_size_in_bytes(to_basis(format), image_size.x, image_size.y);
}

tl::expected<void, DecodeError> Reader::transcode_image(const size_t index, const TranscodeFormat format, std::span<uint8_t> buffer) const {
    DEBUG_ASSERT(index < this->size());
    DEBUG_ASSERT(buffer.size() == this->get_transcoded_size(index, format));

    const std::span<const uint8_t> bytes = this->_basis_file.get();
    const uint32_t byte_count = static_cast<uint32_t>(bytes.size());
    const basist::transcoder_texture_format basis_format = to_basis(format);

    if (!basist::basis_is_format_supported(basis_format, this->_impl->transcoder.get_basis_tex_format(bytes.data(), byte_count))) {
        return tl::unexpected(DecodeError::UnsupportedFormat);
    }

    const size_t output_count = buffer.size() / basist::basis_get_bytes_per_block_or_pixel(basis_format);

    // The transcoder's own state is shared by every call, so each call brings its own to let one page be read from several threads.
    basist::basisu_transcoder_state state;
    const bool transcoded = this->_impl->transcoder.transcode_image_level(
        bytes.data(), byte_count, static_cast<uint32_t>(index), 0, buffer.data(), static_cast<uint32_t>(output_count), basis_format, 0, 0, &state);
    if (!transcoded) {
        return tl::unexpected(DecodeError::TranscodingFailed);
    }

    return {};
}

}
