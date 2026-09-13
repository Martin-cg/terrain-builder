#include <memory>

#include <glm/gtx/component_wise.hpp>
#include <xatlas/xatlas.h>

#include "atlas.h"
#include "containers/FixedArray.h"
#include "enumerate.h"
#include "geometry/geometry.h"
#include "glm_utils.h"
#include "log.h"

namespace uv {

namespace {
using AtlasHandle = std::unique_ptr<xatlas::Atlas, decltype(&xatlas::Destroy)>;

// Normalizing puts a small but real triangle close to the absolute epsilon xatlas judges
// degeneracy against, so the mesh is scaled up to move it clear.
constexpr float DEGENERACY_HEADROOM = 100.0f;

// Flags every triangle thinner than `min_thickness`.
FixedArray<bool> find_thin_triangles(
    const std::span<const glm::uvec3> triangles,
    const std::span<const glm::dvec3> positions,
    const double min_thickness) {
    FixedArray<bool> thin(triangles.size());
    for (const auto [index, triangle] : enumerate(triangles)) {
        thin[index] = geometry::compute_triangle_thickness(geometry::corners(triangle, positions)) < min_thickness;
    }
    return thin;
}

AtlasHandle pack_charts(
    const std::span<const glm::uvec3> triangles,
    const std::span<const glm::dvec3> positions,
    const uint32_t packing_resolution,
    const AtlasOptions &options) {
    // xatlas works in floats, and judges degeneracy against an absolute epsilon.
    radix::geometry::Aabb3d bounds;
    std::vector<glm::vec3> normalized = geometry::to_approximate_normalized(positions, &bounds);
    for (glm::vec3 &position : normalized) {
        position *= DEGENERACY_HEADROOM;
    }

    // The packer spreads about `packing_resolution` texels across the mesh.
    const double texel_size = glm::compMax(bounds.size()) / packing_resolution;
    const FixedArray<bool> thin = find_thin_triangles(triangles, positions, texel_size * options.min_triangle_thickness_in_texels);

    xatlas::MeshDecl declaration;
    declaration.faceIgnoreData = thin.data();
    declaration.vertexCount = normalized.size();
    declaration.vertexPositionData = normalized.data();
    declaration.vertexPositionStride = sizeof(glm::vec3);
    declaration.indexCount = triangles.size() * 3;
    declaration.indexData = triangles.data();
    declaration.indexFormat = xatlas::IndexFormat::UInt32;

    AtlasHandle atlas(xatlas::Create(), xatlas::Destroy);
    const xatlas::AddMeshError error = xatlas::AddMesh(atlas.get(), declaration);
    ASSERT(error == xatlas::AddMeshError::Success, xatlas::StringForEnum(error));

    xatlas::ComputeCharts(atlas.get());

    xatlas::PackOptions pack_options;
    pack_options.padding = options.padding;
    pack_options.resolution = packing_resolution;
    xatlas::PackCharts(atlas.get(), pack_options);

    return atlas;
}
} // namespace

Atlas build_atlas(
    const std::span<const glm::uvec3> triangles,
    const std::span<const glm::dvec3> positions,
    const uint32_t packing_resolution,
    const AtlasOptions options) {
    if (triangles.empty()) {
        return {};
    }

    const AtlasHandle packed = pack_charts(triangles, positions, packing_resolution, options);
    if (packed->width == 0 || packed->height == 0) {
        // Nothing was charted, so there is no atlas to address.
        LOG_WARN("Unwrap produced no charts for {} triangles", triangles.size());
        return {};
    }
    DEBUG_ASSERT(packed->meshCount == 1);
    DEBUG_ASSERT(packed->atlasCount == 1);

    const xatlas::Mesh &mesh = packed->meshes[0];
    DEBUG_ASSERT(mesh.indexCount == triangles.size() * 3);

    const glm::uvec2 atlas_size(packed->width, packed->height);
    const std::span<const xatlas::Vertex> vertices(mesh.vertexArray, mesh.vertexCount);

    Atlas atlas;
    atlas.size = atlas_size;
    atlas.chart_count = mesh.chartCount;

    atlas.uvs.reserve(vertices.size());
    atlas.vertex_map.reserve(vertices.size());
    for (const xatlas::Vertex &vertex : vertices) {
        const glm::dvec2 texel(vertex.uv[0], vertex.uv[1]);
        atlas.uvs.push_back(texel / glm::dvec2(atlas_size));
        atlas.vertex_map.push_back(vertex.xref);
    }

    const std::span<const glm::uvec3> indices = unflatten<3>(std::span<const uint32_t>(mesh.indexArray, mesh.indexCount));
    atlas.triangles.assign(indices.begin(), indices.end());

    for (const auto [triangle_index, triangle] : enumerate(atlas.triangles)) {
        if (vertices[triangle.x].chartIndex < 0) {
            atlas.unmapped_triangles.push_back(triangle_index);
        }
    }
    if (!atlas.unmapped_triangles.empty()) {
        LOG_DEBUG("Left {} of {} triangles out of every chart", atlas.unmapped_triangles.size(), atlas.triangles.size());
    }

    return atlas;
}

} // namespace uv
