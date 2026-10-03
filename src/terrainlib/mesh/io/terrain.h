#pragma once

#include <expected>
#include <filesystem>
#include <vector>

#include "mesh/SimpleMesh.h"
#include "mesh/io/error.h"
#include "mesh/io/options.h"

namespace mesh::io::terrain {

[[nodiscard]]
Result<SimpleMesh> load_from_path(const std::filesystem::path &path, const LoadOptions &options = {});
[[nodiscard]]
Result<SimpleMesh> load_from_buffer(const std::span<const uint8_t> buffer, const LoadOptions &options = {});

[[nodiscard]]
Result<void> save_to_path(const SimpleMesh &mesh, const std::filesystem::path &path, const SaveOptions &options = {});
[[nodiscard]]
Result<std::vector<uint8_t>> save_to_buffer(const SimpleMesh &mesh, const SaveOptions &options = {});

} // namespace mesh::io::terrain
