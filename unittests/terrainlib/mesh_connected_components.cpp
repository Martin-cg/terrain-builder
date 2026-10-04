#include <algorithm>
#include <vector>

#include <glm/glm.hpp>

#include "../catch2_helpers.h"

#include "mesh/SimpleMesh.h"
#include "mesh/connectivity/connected_components.h"

TEST_CASE("mesh::find_connected_components and mesh::count_connected_components group the vertices of connected triangles", "[mesh][connectivity][connected_components]") {
    std::vector<glm::uvec3> triangles;
    size_t vertex_count = 0;
    // Every valid result, as the components may be numbered either way.
    std::vector<mesh::ComponentsIndex> accepted;
    SECTION("no triangles") {
        accepted = {mesh::ComponentsIndex{.vertex_to_component = {}, .component_count = 0}};
    }
    SECTION("triangles sharing no vertex") {
        triangles = {glm::uvec3(0, 1, 2), glm::uvec3(3, 4, 5)};
        vertex_count = 6;
        accepted = {
            mesh::ComponentsIndex{.vertex_to_component = {0, 0, 0, 1, 1, 1}, .component_count = 2},
            mesh::ComponentsIndex{.vertex_to_component = {1, 1, 1, 0, 0, 0}, .component_count = 2}};
    }
    SECTION("triangles sharing a vertex") {
        triangles = {glm::uvec3(0, 1, 2), glm::uvec3(2, 3, 4), glm::uvec3(5, 6, 7)};
        vertex_count = 8;
        accepted = {
            mesh::ComponentsIndex{.vertex_to_component = {0, 0, 0, 0, 0, 1, 1, 1}, .component_count = 2},
            mesh::ComponentsIndex{.vertex_to_component = {1, 1, 1, 1, 1, 0, 0, 0}, .component_count = 2}};
    }
    SECTION("triangles sharing an edge") {
        triangles = {glm::uvec3(0, 1, 2), glm::uvec3(2, 1, 3), glm::uvec3(4, 5, 6)};
        vertex_count = 7;
        accepted = {
            mesh::ComponentsIndex{.vertex_to_component = {0, 0, 0, 0, 1, 1, 1}, .component_count = 2},
            mesh::ComponentsIndex{.vertex_to_component = {1, 1, 1, 1, 0, 0, 0}, .component_count = 2}};
    }
    SECTION("triangles connected through another") {
        // The first two triangles only meet through the last one.
        triangles = {glm::uvec3(0, 1, 2), glm::uvec3(4, 5, 6), glm::uvec3(7, 8, 9), glm::uvec3(2, 3, 4)};
        vertex_count = 10;
        accepted = {
            mesh::ComponentsIndex{.vertex_to_component = {0, 0, 0, 0, 0, 0, 0, 1, 1, 1}, .component_count = 2},
            mesh::ComponentsIndex{.vertex_to_component = {1, 1, 1, 1, 1, 1, 1, 0, 0, 0}, .component_count = 2}};
    }
    SECTION("a vertex no triangle uses") {
        triangles = {glm::uvec3(0, 1, 3)};
        vertex_count = 4;
        accepted = {
            mesh::ComponentsIndex{.vertex_to_component = {0, 0, 1, 0}, .component_count = 2},
            mesh::ComponentsIndex{.vertex_to_component = {1, 1, 0, 1}, .component_count = 2}};
    }
    SECTION("a vertex without any triangle") {
        vertex_count = 1;
        accepted = {mesh::ComponentsIndex{.vertex_to_component = {0}, .component_count = 1}};
    }

    const mesh::ComponentsIndex result = mesh::find_connected_components(triangles, vertex_count);

    CAPTURE(result.vertex_to_component, result.component_count);
    CHECK(std::ranges::contains(accepted, result));
    CHECK(mesh::count_connected_components(triangles, vertex_count) == accepted.front().component_count);
}

TEST_CASE("mesh::split_into_connected_components_with_map gives every component its own mesh", "[mesh][connectivity][connected_components]") {
    SECTION("two components") {
        const SimpleMesh mesh(
            {glm::uvec3(0, 2, 4), glm::uvec3(5, 1, 3)},
            {glm::dvec3(0, 0, 0), glm::dvec3(1, 0, 5), glm::dvec3(2, 0, 0), glm::dvec3(3, 0, 5), glm::dvec3(4, 1, 0), glm::dvec3(5, 1, 5)});

        const mesh::ComponentsAndMap<3, double> result = mesh::split_into_connected_components_with_map(mesh, mesh::find_connected_components(mesh));

        REQUIRE(result.components.size() == 2);
        REQUIRE_FALSE(result.components[0].positions.empty());

        // The components may come in either order.
        const bool is_swapped = result.components[0].positions[0] == glm::dvec3(1, 0, 5);
        const SimpleMesh &even = result.components[is_swapped ? 1 : 0];
        const SimpleMesh &odd = result.components[is_swapped ? 0 : 1];

        CHECK(even.triangles == std::vector<glm::uvec3>{glm::uvec3(0, 1, 2)});
        CHECK(even.positions == std::vector<glm::dvec3>{glm::dvec3(0, 0, 0), glm::dvec3(2, 0, 0), glm::dvec3(4, 1, 0)});
        CHECK(odd.triangles == std::vector<glm::uvec3>{glm::uvec3(2, 0, 1)});
        CHECK(odd.positions == std::vector<glm::dvec3>{glm::dvec3(1, 0, 5), glm::dvec3(3, 0, 5), glm::dvec3(5, 1, 5)});
        CHECK(result.vertex_remap == std::vector<uint32_t>{0, 0, 1, 1, 2, 2});
    }
    SECTION("a vertex no triangle uses") {
        const SimpleMesh mesh(
            {glm::uvec3(0, 1, 3)},
            {glm::dvec3(0, 0, 0), glm::dvec3(1, 0, 0), glm::dvec3(7, 7, 7), glm::dvec3(0, 1, 0)});

        const mesh::ComponentsAndMap<3, double> result = mesh::split_into_connected_components_with_map(mesh, mesh::find_connected_components(mesh));

        REQUIRE(result.components.size() == 2);
        REQUIRE_FALSE(result.components[0].positions.empty());

        // The components may come in either order.
        const bool is_swapped = result.components[0].positions[0] == glm::dvec3(7, 7, 7);
        const SimpleMesh &used = result.components[is_swapped ? 1 : 0];
        const SimpleMesh &unused = result.components[is_swapped ? 0 : 1];

        CHECK(used.triangles == std::vector<glm::uvec3>{glm::uvec3(0, 1, 2)});
        CHECK(used.positions == std::vector<glm::dvec3>{glm::dvec3(0, 0, 0), glm::dvec3(1, 0, 0), glm::dvec3(0, 1, 0)});
        CHECK(unused.triangles.empty());
        CHECK(unused.positions == std::vector<glm::dvec3>{glm::dvec3(7, 7, 7)});
        CHECK(result.vertex_remap == std::vector<uint32_t>{0, 1, 0, 2});
    }
}

TEST_CASE("mesh::split_into_connected_components groups bare triangles by component", "[mesh][connectivity][connected_components]") {
    using Components = std::vector<std::vector<glm::uvec3>>;

    SECTION("two components") {
        const std::vector<glm::uvec3> triangles = {glm::uvec3(0, 2, 4), glm::uvec3(5, 1, 3), glm::uvec3(4, 6, 2)};

        const Components components = mesh::split_into_connected_components(triangles);

        // The components may come in either order.
        CAPTURE(components);
        CHECK((components == Components{{glm::uvec3(0, 2, 4), glm::uvec3(4, 6, 2)}, {glm::uvec3(5, 1, 3)}} ||
               components == Components{{glm::uvec3(5, 1, 3)}, {glm::uvec3(0, 2, 4), glm::uvec3(4, 6, 2)}}));
    }
    SECTION("an index no triangle uses") {
        const std::vector<glm::uvec3> triangles = {glm::uvec3(0, 1, 3)};

        const Components components = mesh::split_into_connected_components(triangles);

        // The components may come in either order.
        CAPTURE(components);
        CHECK((components == Components{{glm::uvec3(0, 1, 3)}, {}} ||
               components == Components{{}, {glm::uvec3(0, 1, 3)}}));
    }
}
