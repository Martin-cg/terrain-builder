#pragma once

#include <string_view>
#include <vector>
#include <span>
#include <type_traits>

#include <opencv2/opencv.hpp>

#include "texture/io.h"
#include "zpp_bits_utils.h"

namespace mesh::io {

struct ImageAndExt {
    cv::Mat image;
    std::string ext;
};

}

#include <zpp_bits.h>

namespace mesh::io {

template <typename Archive>
auto serialize(Archive &archive, const mesh::io::ImageAndExt &item) {
    const auto encode_result = texture::io::encode_to_bytes(item.image, item.ext);
    using Result = decltype(archive(item.ext, encode_result.value()));
    if constexpr (zpp::bits::ArchiveResult<Result>) {
        if (!encode_result.has_value()) {
            return Result(std::errc::bad_message);
        }
    }
    return archive(item.ext, encode_result.value());
}

template <typename Archive>
auto serialize(Archive &archive, mesh::io::ImageAndExt &item) {
    std::vector<uint8_t> encoded;

    auto result = archive(item.ext, encoded);
    if constexpr (zpp::bits::ArchiveResult<decltype(result)>) {
        if (zpp::bits::failure(result)) {
            return result;
        }

        auto decode_result = texture::io::decode_bytes(encoded, cv::IMREAD_UNCHANGED);
        if (!decode_result.has_value()) {
            return decltype(result)(std::errc::bad_message);
        }
        item.image = std::move(decode_result).value();
    }
    return result;
}

} // namespace mesh::io
