#include "io/utils.h"

#include <system_error>

#include <fmt/format.h>

#include "error/utils.h"

namespace io::utils {

alp::Result<void, FileError> create_parent_directories(const std::filesystem::path &path) {
    const std::filesystem::path parent_path = path.parent_path();
    if (parent_path.empty()) {
        return {};
    }

    std::error_code code;
    std::filesystem::create_directories(parent_path, code);
    if (code) {
        return alp::make_unexpected(FileError(code, parent_path), fmt::format("creating directories at {}", parent_path));
    }
    return {};
}

std::filesystem::path canonicalize(const std::filesystem::path &path) {
    std::error_code code;
    std::filesystem::path canonical = std::filesystem::weakly_canonical(path, code);
    if (code) {
        return path;
    }
    return canonical;
}

} // namespace io::utils
