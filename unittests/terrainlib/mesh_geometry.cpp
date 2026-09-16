#include <vector>

#include <glm/glm.hpp>

#include "../catch2_helpers.h"

#include "mesh/geometry.h"

namespace {

// The unit square split into two triangles, all of it in the z = 0 plane facing up.
const std::vector<glm::uvec3> flat_triangles = {{0, 1, 2}, {0, 2, 3}};
const std::vector<glm::dvec3> flat_positions = {{0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0}};

void require_direction(const glm::dvec3 &normal, const glm::dvec3 &expected) {
    const glm::dvec3 unit = glm::normalize(expected);
    REQUIRE_THAT(normal.x, Catch::Matchers::WithinAbs(unit.x, 1e-12));
    REQUIRE_THAT(normal.y, Catch::Matchers::WithinAbs(unit.y, 1e-12));
    REQUIRE_THAT(normal.z, Catch::Matchers::WithinAbs(unit.z, 1e-12));
}

} // namespace

TEST_CASE("compute_vertex_normals gives a flat surface one normal everywhere", "[terrainlib][mesh_geometry]") {
    const std::vector<glm::dvec3> normals = mesh::compute_vertex_normals(flat_triangles, flat_positions);

    REQUIRE(normals.size() == flat_positions.size());
    for (const glm::dvec3 &normal : normals) {
        require_direction(normal, {0, 0, 1});
    }
}

TEST_CASE("compute_vertex_normals leaves a vertex no triangle touches at zero", "[terrainlib][mesh_geometry]") {
    std::vector<glm::dvec3> positions = flat_positions;
    positions.push_back({5, 5, 5});

    const std::vector<glm::dvec3> normals = mesh::compute_vertex_normals(flat_triangles, positions);

    REQUIRE(normals.size() == positions.size());
    REQUIRE(normals.back() == glm::dvec3(0));
}

TEST_CASE("compute_vertex_normals averages the faces meeting at a vertex", "[terrainlib][mesh_geometry]") {
    // A right angle folded along the x axis: one face in z = 0, one in y = 0, both turning
    // through 90 degrees at the shared origin.
    const std::vector<glm::uvec3> triangles = {{0, 1, 2}, {0, 3, 1}};
    const std::vector<glm::dvec3> positions = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}, {0, 0, 1}};

    const std::vector<glm::dvec3> normals = mesh::compute_vertex_normals(triangles, positions);

    // Equal angles, so the shared vertex sits on the bisector of the two face normals.
    require_direction(normals[0], {0, 1, 1});
}

TEST_CASE("compute_vertex_normals weights each face by the angle it turns through", "[terrainlib][mesh_geometry]") {
    // The same fold, but the second face meets the origin at 45 degrees rather than 90. Both faces
    // still have equal area, so an area weighted average could not tell this apart from the bisector.
    const std::vector<glm::uvec3> triangles = {{0, 1, 2}, {0, 3, 1}};
    const std::vector<glm::dvec3> positions = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}, {1, 0, 1}};

    const std::vector<glm::dvec3> normals = mesh::compute_vertex_normals(triangles, positions);

    // 90 degrees of (0, 0, 1) against 45 degrees of (0, 1, 0).
    require_direction(normals[0], {0, 0.5, 1});
}
