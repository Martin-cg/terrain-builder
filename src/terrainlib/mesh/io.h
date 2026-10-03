#pragma once

#include <expected>
#include <filesystem>

#include "mesh/SimpleMesh.h"
#include "mesh/io/options.h"
#include "mesh/io/error.h"

namespace mesh::io {

[[nodiscard]]
Result<SimpleMesh> load_from_path(
    const std::filesystem::path &path,
    const LoadOptions& options = {});

[[nodiscard]]
Result<void> save_to_path(
    const SimpleMesh &mesh,
    const std::filesystem::path &path,
    const SaveOptions& options = {});

}
