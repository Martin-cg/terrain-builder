#pragma once

#include <filesystem>
#include <optional>

#include <radix/geometry.h>

#include "Dataset.h"
#include "error/Result.h"
#include "gdal/CplError.h"
#include "octree/Id.h"
#include "mesh/SimpleMesh.h"
#include "mesh/io/error.h"
#include "tile_provider.h"

namespace terrainbuilder {

alp::Result<void, gdal::CplError, TileError, mesh::io::Error> build_and_save_patch(
    Dataset &dataset,
    const OGRSpatialReference &target_bounds_srs,
    const radix::geometry::Aabb3d &target_bounds,
    const OGRSpatialReference &texture_srs,
    const TileProvider *tile_provider,
    const OGRSpatialReference &mesh_srs,
    const std::filesystem::path &output_path);

// The mesh for the target bounds, or nothing if the dataset holds no terrain there.
alp::Result<std::optional<SimpleMesh>, gdal::CplError, TileError> build_patch(
    Dataset &dataset,
    const OGRSpatialReference &target_bounds_srs,
    const radix::geometry::Aabb3d &target_bounds,
    const OGRSpatialReference &texture_srs,
    const TileProvider *tile_provider,
    const OGRSpatialReference &mesh_srs);

alp::Result<void> build_all_patches(
    Dataset &dataset,
    const octree::Id::Level target_level,
    const OGRSpatialReference &texture_srs,
    const TileProvider *tile_provider,
    const OGRSpatialReference &mesh_srs,
    const std::filesystem::path &output_base_path,
    const std::string &output_format,
    const bool overwrite_existing);
}
