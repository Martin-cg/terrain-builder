#pragma once

#include <filesystem>

#include "error/Result.h"
#include "io/FileError.h"

namespace io::utils {

// Creates the missing parent directories of `path`.
[[nodiscard]]
alp::Result<void, FileError> create_parent_directories(const std::filesystem::path &path);

} // namespace io::utils
