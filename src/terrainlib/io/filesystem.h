#pragma once

#include <concepts>
#include <filesystem>
#include <system_error>

#include <fmt/format.h>

#include "error/Context.h"
#include "error/Result.h"
#include "error/utils.h"
#include "fmt_impls.h"
#include "io/FileError.h"

namespace io::filesystem {

// Whether something exists at `path`, following symlinks.
[[nodiscard]]
inline alp::Result<bool, FileError> exists(const std::filesystem::path &path) {
    std::error_code code;
    const std::filesystem::file_status status = std::filesystem::status(path, code);
    if (status.type() == std::filesystem::file_type::none) {
        return alp::make_unexpected(FileError(code, path), alp::make_context("reading a file status at {}", path));
    }
    return std::filesystem::exists(status);
}

// Whether `path` names a directory; false if nothing exists at `path`.
[[nodiscard]]
inline alp::Result<bool, FileError> is_directory(const std::filesystem::path &path) {
    std::error_code code;
    const std::filesystem::file_status status = std::filesystem::status(path, code);
    if (status.type() == std::filesystem::file_type::none) {
        return alp::make_unexpected(FileError(code, path), alp::make_context("reading a file status at {}", path));
    }
    return std::filesystem::is_directory(status);
}

// Whether `path` names a regular file; false if nothing exists at `path`.
[[nodiscard]]
inline alp::Result<bool, FileError> is_regular_file(const std::filesystem::path &path) {
    std::error_code code;
    const std::filesystem::file_status status = std::filesystem::status(path, code);
    if (status.type() == std::filesystem::file_type::none) {
        return alp::make_unexpected(FileError(code, path), alp::make_context("reading a file status at {}", path));
    }
    return std::filesystem::is_regular_file(status);
}

// Whether `path` itself is a symlink, without following it; false if nothing exists at `path`.
[[nodiscard]]
inline alp::Result<bool, FileError> is_symlink(const std::filesystem::path &path) {
    std::error_code code;
    const std::filesystem::file_status status = std::filesystem::symlink_status(path, code);
    if (status.type() == std::filesystem::file_type::none) {
        return alp::make_unexpected(FileError(code, path), alp::make_context("reading a file status at {}", path));
    }
    return std::filesystem::is_symlink(status);
}

// Calls `visit` with each regular file and symlink below `path`, stopping early once `visit` returns false.
template <std::predicate<const std::filesystem::path &> Visit>
[[nodiscard]]
alp::Result<void, FileError> visit_files_recursively(const std::filesystem::path &path, Visit &&visit) {
    const auto context = alp::make_context("listing files at {}", path);

    std::error_code code;
    std::filesystem::recursive_directory_iterator iterator(path, code);
    if (code) {
        return alp::make_unexpected(FileError(code, path), context);
    }

    const std::filesystem::recursive_directory_iterator end;
    while (iterator != end) {
        const std::filesystem::directory_entry &entry = *iterator;
        const std::filesystem::file_status status = entry.symlink_status(code);
        if (status.type() == std::filesystem::file_type::none) {
            return alp::make_unexpected(FileError(code, entry.path()), context);
        }
        if (std::filesystem::is_regular_file(status) || std::filesystem::is_symlink(status)) {
            if (!visit(entry.path())) {
                return {};
            }
        }

        iterator.increment(code);
        if (code) {
            return alp::make_unexpected(FileError(code, path), context);
        }
    }
    return {};
}

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
