#include <array>
#include <vector>

#include "../catch2_helpers.h"
#include "mesh/combine.h"

TEST_CASE("mesh::combine and mesh::combine_inplace append the second mesh behind the first", "[mesh][combine]") {
    const SimpleMesh first(
        {glm::uvec3(0, 1, 2), glm::uvec3(2, 1, 3)},
        {glm::dvec3(0, 0, 0), glm::dvec3(1, 0, 0), glm::dvec3(0, 1, 0), glm::dvec3(1, 1, 0)},
        {glm::dvec2(0.1, 0.1), glm::dvec2(0.2, 0.1), glm::dvec2(0.1, 0.2), glm::dvec2(0.2, 0.2)});
    const SimpleMesh second(
        {glm::uvec3(2, 0, 1)},
        {glm::dvec3(2, 0, 5), glm::dvec3(3, 0, 5), glm::dvec3(2, 1, 5)},
        {glm::dvec2(0.6, 0.7), glm::dvec2(0.8, 0.7), glm::dvec2(0.6, 0.9)});

    SimpleMesh result;
    SECTION("combine") {
        result = mesh::combine(std::array{first, second});
    }
    SECTION("combine_inplace") {
        result = first;
        mesh::combine_inplace(result, second);
    }

    CHECK(result.triangles == std::vector<glm::uvec3>{glm::uvec3(0, 1, 2), glm::uvec3(2, 1, 3), glm::uvec3(6, 4, 5)});
    CHECK(result.positions == std::vector<glm::dvec3>{
        glm::dvec3(0, 0, 0), glm::dvec3(1, 0, 0), glm::dvec3(0, 1, 0), glm::dvec3(1, 1, 0),
        glm::dvec3(2, 0, 5), glm::dvec3(3, 0, 5), glm::dvec3(2, 1, 5)});
    CHECK(result.uvs == std::vector<glm::dvec2>{
        glm::dvec2(0.1, 0.1), glm::dvec2(0.2, 0.1), glm::dvec2(0.1, 0.2), glm::dvec2(0.2, 0.2),
        glm::dvec2(0.6, 0.7), glm::dvec2(0.8, 0.7), glm::dvec2(0.6, 0.9)});
}

TEST_CASE("mesh::combine and mesh::combine_inplace of meshes without uvs have no uvs", "[mesh][combine]") {
    const SimpleMesh first(
        {glm::uvec3(0, 1, 2)},
        {glm::dvec3(0, 0, 0), glm::dvec3(1, 0, 0), glm::dvec3(0, 1, 0)});
    const SimpleMesh second(
        {glm::uvec3(1, 2, 0)},
        {glm::dvec3(2, 0, 5), glm::dvec3(3, 0, 5), glm::dvec3(2, 1, 5)});

    SimpleMesh result;
    SECTION("combine") {
        result = mesh::combine(std::array{first, second});
    }
    SECTION("combine_inplace") {
        result = first;
        mesh::combine_inplace(result, second);
    }

    CHECK(result.triangles == std::vector<glm::uvec3>{glm::uvec3(0, 1, 2), glm::uvec3(4, 5, 3)});
    CHECK(result.positions == std::vector<glm::dvec3>{
        glm::dvec3(0, 0, 0), glm::dvec3(1, 0, 0), glm::dvec3(0, 1, 0),
        glm::dvec3(2, 0, 5), glm::dvec3(3, 0, 5), glm::dvec3(2, 1, 5)});
    CHECK(result.uvs.empty());
}

TEST_CASE("mesh::combine and mesh::combine_inplace take the uvs of a mesh appended to an empty one", "[mesh][combine]") {
    const SimpleMesh empty;
    const SimpleMesh textured(
        {glm::uvec3(2, 0, 1)},
        {glm::dvec3(2, 0, 5), glm::dvec3(3, 0, 5), glm::dvec3(2, 1, 5)},
        {glm::dvec2(0.6, 0.7), glm::dvec2(0.8, 0.7), glm::dvec2(0.6, 0.9)});

    SimpleMesh result;
    SECTION("combine") {
        result = mesh::combine(std::array{empty, textured});
    }
    SECTION("combine_inplace") {
        result = empty;
        mesh::combine_inplace(result, textured);
    }

    CHECK(result.triangles == std::vector<glm::uvec3>{glm::uvec3(2, 0, 1)});
    CHECK(result.positions == std::vector<glm::dvec3>{glm::dvec3(2, 0, 5), glm::dvec3(3, 0, 5), glm::dvec3(2, 1, 5)});
    CHECK(result.uvs == std::vector<glm::dvec2>{glm::dvec2(0.6, 0.7), glm::dvec2(0.8, 0.7), glm::dvec2(0.6, 0.9)});
}

TEST_CASE("mesh::combine reports the first vertex of every mesh", "[mesh][combine]") {
    const SimpleMesh first(
        {glm::uvec3(0, 1, 2)},
        {glm::dvec3(0, 0, 0), glm::dvec3(1, 0, 0), glm::dvec3(0, 1, 0)});
    const SimpleMesh second(
        {glm::uvec3(3, 1, 2)},
        {glm::dvec3(2, 0, 5), glm::dvec3(3, 0, 5), glm::dvec3(2, 1, 5), glm::dvec3(3, 1, 5)});
    const SimpleMesh third(
        {glm::uvec3(1, 2, 0)},
        {glm::dvec3(4, 0, 9), glm::dvec3(5, 0, 9), glm::dvec3(4, 1, 9)});

    std::vector<size_t> vertex_offsets = {11};
    const SimpleMesh result = mesh::combine(std::array{first, second, third}, vertex_offsets);

    CHECK(vertex_offsets == std::vector<size_t>{0, 3, 7});
    CHECK(result.triangles == std::vector<glm::uvec3>{glm::uvec3(0, 1, 2), glm::uvec3(6, 4, 5), glm::uvec3(8, 9, 7)});
}
