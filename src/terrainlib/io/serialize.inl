#pragma once

#include <fmt/format.h>
#include <zpp_bits.h>

#include "error/try.h"
#include "error/utils.h"
#include "io/bytes.h"

namespace io {

template <typename T>
alp::Result<std::vector<uint8_t>, SerdeError> write_to_bytes(const T &value) {
    std::vector<uint8_t> data;
    zpp::bits::out out(data);
    const auto result = out(value);
    if (zpp::bits::failure(result)) {
        return alp::make_unexpected(SerdeError(result));
    }
    return data;
}

template <typename T>
alp::Result<T, SerdeError> read_from_bytes(const std::span<const uint8_t> bytes) {
    zpp::bits::in in(bytes);
    T value = zpp::bits::access::make<T>();
    const auto result = in(value);
    if (zpp::bits::failure(result)) {
        return alp::make_unexpected(SerdeError(result));
    }
    return value;
}

template <typename T>
Result<T> read_from_path(const std::filesystem::path &path) {
    ALP_TRY(const std::vector<uint8_t> &bytes, read_bytes_from_path(path));
    ALP_TRY_CONTEXT(T value, read_from_bytes<T>(bytes), fmt::format("decoding the data in {}", path.string()));
    return value;
}

template <typename T>
Result<void> write_to_path(const T &value, const std::filesystem::path &path, bool make_dirs) {
    ALP_TRY_CONTEXT(const std::vector<uint8_t> &bytes, write_to_bytes(value), fmt::format("encoding the data for {}", path.string()));
    ALP_TRY(write_bytes_to_path(bytes, path, make_dirs));
    return {};
}

} // namespace io
