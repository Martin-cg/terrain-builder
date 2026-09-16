#pragma once

#include <cstdint>
#include <filesystem>
#include <span>
#include <vector>

#include "io/Error.h"

namespace io {

// Writes `bytes` to the file at `path`.
// Creates the parent directories first if `make_dirs` is set.
[[nodiscard]]
alp::Result<void, FileError> write_bytes_to_path(std::span<const uint8_t> bytes, const std::filesystem::path &path, bool make_dirs = true);

// Reads the whole file at `path`.
[[nodiscard]]
alp::Result<std::vector<uint8_t>, FileError> read_bytes_from_path(const std::filesystem::path &path);

} // namespace io
