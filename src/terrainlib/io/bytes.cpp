#include <fstream>
#include <system_error>

#include <fmt/format.h>

#include "error/try.h"
#include "error/utils.h"
#include "io/bytes.h"
#include "io/utils.h"
#include "log.h"

namespace io {

alp::Result<void, FileError> write_bytes_to_path(const std::span<const uint8_t> bytes, const std::filesystem::path &path, bool make_dirs) {
    LOG_TRACE("Writing bytes to path {}", path);
    const auto context = [&] {
        return fmt::format("writing bytes to {}", path.string());
    };

    if (make_dirs) {
        ALP_TRY_CONTEXT(utils::create_parent_directories(path), context());
    }

    // A stream reports no reason for a failure, so it becomes a general io error.
    std::ofstream file(path, std::ios::binary);
    file.write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!file.good()) {
        return alp::make_unexpected(FileError(std::make_error_code(std::errc::io_error), path), context());
    }

    return {};
}

alp::Result<std::vector<uint8_t>, FileError> read_bytes_from_path(const std::filesystem::path &path) {
    LOG_TRACE("Reading bytes from path {}", path);
    const auto context = [&] {
        return fmt::format("reading bytes from {}", path.string());
    };

    std::error_code code;
    const std::uintmax_t size = std::filesystem::file_size(path, code);
    if (code) {
        return alp::make_unexpected(FileError(code, path), context());
    }

    // A stream reports no reason for a failure, so it becomes a general io error.
    std::vector<uint8_t> buffer(static_cast<size_t>(size));
    std::ifstream file(path, std::ios::binary);
    file.read(reinterpret_cast<char *>(buffer.data()), static_cast<std::streamsize>(buffer.size()));
    if (!file.good()) {
        return alp::make_unexpected(FileError(std::make_error_code(std::errc::io_error), path), context());
    }

    return buffer;
}

} // namespace io
