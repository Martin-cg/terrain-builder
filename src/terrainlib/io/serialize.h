#pragma once

#include <cstdint>
#include <filesystem>
#include <span>
#include <vector>

#include "io/Error.h"

namespace io {

// The bytes zpp_bits encodes `value` as.
template <typename T>
[[nodiscard]]
alp::Result<std::vector<uint8_t>, SerdeError> write_to_bytes(const T &value);

// The value zpp_bits decodes from `bytes`.
template <typename T>
[[nodiscard]]
alp::Result<T, SerdeError> read_from_bytes(std::span<const uint8_t> bytes);

// Writes `value`, encoded as bytes, to the file at `path`.
// Creates the parent directories first if `make_dirs` is set.
template <typename T>
[[nodiscard]]
Result<void> write_to_path(const T &value, const std::filesystem::path &path, bool make_dirs = true);

// Reads the value encoded in the file at `path`.
template <typename T>
[[nodiscard]]
Result<T> read_from_path(const std::filesystem::path &path);

} // namespace io

#include "io/serialize.inl"
