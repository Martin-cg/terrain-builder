#pragma once

#include <filesystem>
#include <system_error>

#include <fmt/format.h>

#include "error/Context.h"
#include "error/Result.h"
#include "error/utils.h"
#include "io/FileError.h"

namespace io::filesystem {

// Creates the directory at `path` and its missing parents.
[[nodiscard]]
inline alp::Result<void, FileError> create_directories(const std::filesystem::path &path) {
    std::error_code code;
    std::filesystem::create_directories(path, code);
    if (code) {
        return alp::make_unexpected(FileError(code, path), alp::make_context("creating directories at {}", path));
    }
    return {};
}

// Creates the missing parent directories of `path`.
[[nodiscard]]
inline alp::Result<void, FileError> create_parent_directories(const std::filesystem::path &path) {
    const std::filesystem::path parent_path = path.parent_path();
    if (parent_path.empty()) {
        return {};
    }
    return filesystem::create_directories(parent_path);
}

// The canonical form of `path`, or `path` as given if it cannot be made canonical.
[[nodiscard]]
inline std::filesystem::path canonicalize(const std::filesystem::path &path) {
    std::error_code code;
    std::filesystem::path canonical = std::filesystem::weakly_canonical(path, code);
    if (code) {
        return path;
    }
    return canonical;
}

} // namespace io::filesystem
