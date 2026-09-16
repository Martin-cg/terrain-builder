#pragma once

#include <filesystem>
#include <vector>
#include <memory>

#include <cgltf_write.h>
#include <tl/expected.hpp>

#include "mesh/SimpleMesh.h"
#include "mesh/io/error.h"
#include "mesh/io/options.h"

namespace mesh::io::gltf {

using RawMesh = std::unique_ptr<cgltf_data, decltype(&cgltf_free)>;

[[nodiscard]]
Result<SimpleMesh> load_from_path(
    const std::filesystem::path &path,
    const LoadOptions &options = {});
[[nodiscard]]
Result<SimpleMesh> load_from_raw(
    const RawMesh &mesh,
    const LoadOptions &options = {});

[[nodiscard]]
Result<void> save_to_path(
    const SimpleMesh &mesh,
    const std::filesystem::path &path,
    const SaveOptions &options = {});
// Result<SimpleMesh> save_to_raw(const RawMesh &mesh, const SaveOptions &options = {});

[[nodiscard]]
alp::Result<RawMesh, GltfError> load_raw_from_path(const std::filesystem::path &path);

} // namespace mesh::io::gltf
