#include <functional>
#include <span>
#include <vector>

#include "../catch2_helpers.h"

#include "mesh/SimpleMesh.h"
#include "mesh/bounds.h"

TEST_CASE("mesh::calculate_bounds", "[mesh][bounds]") {
    SimpleMesh mesh;
    mesh.positions = {
        glm::dvec3(3.0, -1.0, 0.5),
        glm::dvec3(-2.0, 5.0, 1.0),
        glm::dvec3(0.0, 0.0, -4.0)};

    const auto bounds = mesh::calculate_bounds(mesh);
    CHECK(bounds.min == glm::dvec3(-2.0, -1.0, -4.0));
    CHECK(bounds.max == glm::dvec3(3.0, 5.0, 1.0));
}

TEST_CASE("mesh::calculate_bounds combines several meshes", "[mesh][bounds]") {
    SimpleMesh first;
    first.positions = {
        glm::dvec3(0.0, 0.0, 0.0),
        glm::dvec3(1.0, -2.0, 0.0)};
    SimpleMesh second;
    second.positions = {
        glm::dvec3(-3.0, 1.0, 0.0),
        glm::dvec3(0.0, 0.0, 5.0)};
    const std::vector<std::reference_wrapper<const SimpleMesh>> meshes = {first, second};

    const auto bounds = mesh::calculate_bounds(std::span<const std::reference_wrapper<const SimpleMesh>>(meshes));
    CHECK(bounds.min == glm::dvec3(-3.0, -2.0, 0.0));
    CHECK(bounds.max == glm::dvec3(1.0, 1.0, 5.0));
}
