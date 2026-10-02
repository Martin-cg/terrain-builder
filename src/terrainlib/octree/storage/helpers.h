#pragma once

#include <expected>
#include <memory>
#include <optional>
#include <filesystem>

#include "io/Error.h"
#include "octree/IndexMap.h"
#include "octree/disk/IndexFile.h"
#include "octree/disk/Layout.h"
#include "octree/disk/layout/Strategy.h"

namespace octree::helpers {

struct LayoutWithoutBase {
    std::unique_ptr<disk::layout::Strategy> strategy;
    std::string extension_with_dot;
};

[[nodiscard]]
alp::Result<std::optional<LayoutWithoutBase>, io::FileError> guess_layout_strategy(
    const std::filesystem::path &base_path,
    size_t max_files_to_check = 100);
[[nodiscard]]
std::expected<void, io::Error> save_index_map(const IndexMap &index, const disk::Layout &layout);
[[nodiscard]]
alp::Result<IndexMap, io::FileError> build_index_map(const disk::Layout &layout);

}
