#pragma once

#include <filesystem>
#include <optional>
#include <source_location>
#include <string_view>
#include <system_error>
#include <vector>

#include <tl/expected.hpp>

#include "mesh/io.h"

namespace octree {

struct MeshCodec {
    using value_type = mesh::Simple;
    using load_error = mesh::io::Error;
    using save_error = mesh::io::Error;

    static tl::expected<value_type, load_error> load_from_path(const std::filesystem::path& path) {
        return mesh::io::load_from_path(path);
    }

    static tl::expected<void, save_error> save_to_path(const value_type& value, const std::filesystem::path& path) {
        return mesh::io::save_to_path(value, path);
    }

    // Creates the error a load reports for a missing file at `path`.
    static load_error file_not_found(const std::filesystem::path &path, const std::source_location location = std::source_location::current()) {
        return load_error(::io::FileError(std::make_error_code(std::errc::no_such_file_or_directory), path), location);
    }

    static std::vector<std::string_view> supported_extensions() {
        return {".terrain", ".glb", ".gltf"};
    }
};

} // namespace octree
