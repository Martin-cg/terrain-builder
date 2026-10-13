#pragma once

#include <filesystem>
#include <optional>
#include <string_view>
#include <vector>

#include "error/Result.h"
#include "mesh/io.h"

namespace octree {

struct MeshCodec {
    using value_type = mesh::Simple;
    using load_error = mesh::io::Error;
    using save_error = mesh::io::Error;

    [[nodiscard]]
    static alp::Result<value_type, load_error> load_from_path(const std::filesystem::path& path) {
        return mesh::io::load_from_path(path);
    }

    [[nodiscard]]
    static alp::Result<void, save_error> save_to_path(const value_type& value, const std::filesystem::path& path) {
        return mesh::io::save_to_path(value, path);
    }

    static std::vector<std::string_view> supported_extensions() {
        return {".terrain", ".glb", ".gltf"};
    }
};

} // namespace octree
