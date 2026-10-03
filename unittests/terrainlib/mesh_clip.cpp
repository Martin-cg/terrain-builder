#include <algorithm>
#include <cmath>
#include <iterator>
#include <utility>
#include <vector>

#include <glm/glm.hpp>

#include <catch2/catch_test_macros.hpp>

#include "../catch2_helpers.h"

#include "mesh/SimpleMesh.h"
#include "mesh/TriangleSoup.h"
#include "mesh/clip.h"
#include "mesh/connectivity/manifold.h"
#include "mesh/geometry.h"

TEST_CASE("mesh::clip_on_bounds of an empty mesh is empty", "[mesh][clip]") {
    const SimpleMesh mesh;
    const radix::geometry::Aabb3d bounds(glm::dvec3(-1.0, -1.0, -1.0), glm::dvec3(1.0, 1.0, 1.0));
    const SimpleMesh clipped_mesh = mesh::clip_on_bounds(mesh, bounds).into_owned();
    CHECK(clipped_mesh.positions.empty());
    CHECK(clipped_mesh.triangles.empty());
    CHECK(clipped_mesh.uvs.empty());
    CHECK(!clipped_mesh.texture.has_value());
}

TEST_CASE("mesh::clip_on_bounds keeps a triangle inside the bounds unchanged", "[mesh][clip]") {
    SimpleMesh mesh;
    mesh.positions = {
        glm::dvec3(0, 0, 0),
        glm::dvec3(1, 0, 0),
        glm::dvec3(0, 1, 0),
    };
    mesh.uvs = {
        glm::dvec2(0, 0),
        glm::dvec2(1, 0),
        glm::dvec2(0, 1),
    };
    mesh.triangles = {
        glm::uvec3(0, 1, 2)};
    const radix::geometry::Aabb3d bounds(glm::dvec3(-1, -1, -1), glm::dvec3(1, 1, 1));
    const SimpleMesh clipped_mesh = mesh::clip_on_bounds(mesh, bounds).into_owned();
    CHECK(mesh.positions == clipped_mesh.positions);
    CHECK(mesh.uvs == clipped_mesh.uvs);
    CHECK(mesh.triangles == clipped_mesh.triangles);
}

TEST_CASE("mesh::clip_on_bounds removes a triangle outside the bounds", "[mesh][clip]") {
    SimpleMesh mesh;
    mesh.positions = {
        glm::dvec3(0, 0, 0),
        glm::dvec3(1, 0, 0),
        glm::dvec3(0, 1, 0),
    };
    mesh.uvs = {
        glm::dvec2(0, 0),
        glm::dvec2(1, 0),
        glm::dvec2(0, 1),
    };
    mesh.triangles = {
        glm::uvec3(0, 1, 2)};
    const radix::geometry::Aabb3d bounds(glm::dvec3(-1, -1, -1), glm::dvec3(-0.1, -0.1, -0.1));
    const SimpleMesh clipped_mesh = mesh::clip_on_bounds(mesh, bounds).into_owned();
    CHECK(clipped_mesh.positions == std::vector<glm::dvec3>{});
    CHECK(clipped_mesh.uvs == std::vector<glm::dvec2>{});
    CHECK(clipped_mesh.triangles == std::vector<glm::uvec3>{});
}

TEST_CASE("mesh::clip_on_bounds removes a triangle touching the bounds in one point", "[mesh][clip]") {
    SimpleMesh mesh;
    mesh.positions = {
        glm::dvec3(0, 0, 0),
        glm::dvec3(1, 0, 0),
        glm::dvec3(0, 1, 0),
    };
    mesh.uvs = {
        glm::dvec2(0, 0),
        glm::dvec2(1, 0),
        glm::dvec2(0, 1),
    };
    mesh.triangles = {
        glm::uvec3(0, 1, 2)};
    const radix::geometry::Aabb3d bounds(glm::dvec3(-1, -1, -1), glm::dvec3(0, 0, 0));
    const SimpleMesh clipped_mesh = mesh::clip_on_bounds(mesh, bounds).into_owned();
    CHECK(clipped_mesh.positions == std::vector<glm::dvec3>{});
    CHECK(clipped_mesh.uvs == std::vector<glm::dvec2>{});
    CHECK(clipped_mesh.triangles == std::vector<glm::uvec3>{});
}

TEST_CASE("mesh::clip_on_bounds cuts a triangle lying in a face of the bounds", "[mesh][clip]") {
    SimpleMesh mesh;
    mesh.positions = {
        glm::dvec3(0, 0, 0),
        glm::dvec3(1, 0, 0),
        glm::dvec3(0, 1, 0),
    };
    mesh.triangles = {
        glm::uvec3(0, 1, 2)};
    const radix::geometry::Aabb3d bounds(glm::dvec3(-1, -1, -1), glm::dvec3(0.5, 0.5, 0));
    const SimpleMesh clipped_mesh = mesh::clip_on_bounds(mesh, bounds).into_owned();
    const TriangleSoup clipped_soup = to_sorted_triangle_soup(clipped_mesh);

    const TriangleSoup expected1 = {
        {glm::dvec3(0, 0, 0), glm::dvec3(0.5, 0, 0), glm::dvec3(0.5, 0.5, 0)},
        {glm::dvec3(0, 0, 0), glm::dvec3(0.5, 0.5, 0), glm::dvec3(0, 0.5, 0)}};

    const TriangleSoup expected2 = {
        {glm::dvec3(0, 0, 0), glm::dvec3(0.5, 0, 0), glm::dvec3(0, 0.5, 0)},
        {glm::dvec3(0, 0.5, 0), glm::dvec3(0.5, 0, 0), glm::dvec3(0.5, 0.5, 0)}};

    CAPTURE(clipped_soup);
    CHECK((clipped_soup == expected1 || clipped_soup == expected2));
}

TEST_CASE("mesh::clip_on_bounds cuts a triangle to the square whose corner lies on its hypotenuse", "[mesh][clip]") {
    SimpleMesh mesh;
    mesh.positions = {
        glm::dvec3(0, 0, 0),
        glm::dvec3(1, 0, 0),
        glm::dvec3(0, 1, 0),
    };
    mesh.triangles = {
        glm::uvec3(0, 1, 2)};
    const radix::geometry::Aabb3d bounds(glm::dvec3(-1, -1, -1), glm::dvec3(0.5, 0.5, 0.5));
    const SimpleMesh clipped_mesh = mesh::clip_on_bounds(mesh, bounds).into_owned();
    const TriangleSoup clipped_soup = to_sorted_triangle_soup(clipped_mesh);

    const TriangleSoup expected1 = {
        {glm::dvec3(0, 0, 0), glm::dvec3(0.5, 0, 0), glm::dvec3(0.5, 0.5, 0)},
        {glm::dvec3(0, 0, 0), glm::dvec3(0.5, 0.5, 0), glm::dvec3(0, 0.5, 0)}};

    const TriangleSoup expected2 = {
        {glm::dvec3(0, 0, 0), glm::dvec3(0.5, 0, 0), glm::dvec3(0, 0.5, 0)},
        {glm::dvec3(0, 0.5, 0), glm::dvec3(0.5, 0, 0), glm::dvec3(0.5, 0.5, 0)}};

    CAPTURE(clipped_soup);
    CHECK((clipped_soup == expected1 || clipped_soup == expected2));
}

TEST_CASE("mesh::clip_on_bounds removes a triangle with an edge on the bounds and the rest outside", "[mesh][clip]") {
    SimpleMesh mesh;
    mesh.positions = {
        glm::dvec3(0.0, 0.0, 0.0),  // on plane y = 0
        glm::dvec3(1.0, 0.0, 0.0),  // on plane y = 0
        glm::dvec3(0.5, -1.0, 0.0), // outside (below plane)
    };
    mesh.triangles = {
        glm::uvec3(0, 1, 2)};

    // Clip with bottom plane y >= 0
    const radix::geometry::Aabb3d bounds(
        glm::dvec3(-1.0, 0.0, -1.0),
        glm::dvec3(2.0, 1.0, 1.0));

    const SimpleMesh clipped_mesh = mesh::clip_on_bounds(mesh, bounds).into_owned();
    const TriangleSoup clipped_soup = to_sorted_triangle_soup(clipped_mesh);

    CAPTURE(clipped_soup);
    CHECK(clipped_soup == TriangleSoup{});
}

TEST_CASE("mesh::clip_on_bounds cuts a triangle enclosing the bounds to the bounds", "[mesh][clip]") {
    SimpleMesh mesh;
    mesh.positions = {
        glm::dvec3(0.0, 3.0, 0.0),
        glm::dvec3(3.0, -2.0, 0.0),
        glm::dvec3(-3.0, -2.0, 0.0),
    };
    mesh.triangles = {glm::uvec3(0, 1, 2)};

    const radix::geometry::Aabb3d bounds(
        glm::dvec3(-1.0),
        glm::dvec3(1.0));
    const radix::geometry::Aabb2d bounds2d(
        glm::dvec2(bounds.min),
        glm::dvec2(bounds.max));

    const SimpleMesh clipped_mesh = mesh::clip_on_bounds(mesh, bounds).into_owned();
    const TriangleSoup clipped_soup = to_sorted_triangle_soup(clipped_mesh);

    double total_area = 0.0;
    for (const auto &triangle : clipped_soup) {
        const auto &a = triangle[0];
        const auto &b = triangle[1];
        const auto &c = triangle[2];

        CAPTURE(a, b, c);

        // Still on the source plane.
        CHECK(a.z == 0.0);
        CHECK(b.z == 0.0);
        CHECK(c.z == 0.0);

        // Every emitted vertex must lie inside the clipping bounds.
        CHECK(bounds2d.contains_inclusive(glm::dvec2(a)));
        CHECK(bounds2d.contains_inclusive(glm::dvec2(b)));
        CHECK(bounds2d.contains_inclusive(glm::dvec2(c)));

        total_area += geometry::compute_triangle_area(a, b, c);
    }

    // The clipped region is exactly a 2x2 square.
    CHECK_THAT(total_area, WithinEpsilon(4.0));
}

TEST_CASE("mesh::clip_on_bounds cuts off the corner of a triangle with two vertices in bounds", "[mesh][clip]") {
    SimpleMesh mesh;
    mesh.positions = {
        glm::dvec3(-0.5, 0, 0),
        glm::dvec3(0.5, 0, 0),
        glm::dvec3(0.5, 1, 0),
    };
    mesh.triangles = {
        glm::uvec3(0, 1, 2)};
    const radix::geometry::Aabb3d bounds(glm::dvec3(-1, -1, -1), glm::dvec3(0.5, 0.5, 0.5));
    const SimpleMesh clipped_mesh = mesh::clip_on_bounds(mesh, bounds).into_owned();
    const TriangleSoup clipped_soup = to_sorted_triangle_soup(clipped_mesh);

    const TriangleSoup expected1 = {
        {glm::dvec3(-0.5, 0, 0), glm::dvec3(0.5, 0, 0), glm::dvec3(0.5, 0.5, 0)},
        {glm::dvec3(-0.5, 0, 0), glm::dvec3(0.5, 0.5, 0), glm::dvec3(0.0, 0.5, 0)}};

    const TriangleSoup expected2 = {
        {glm::dvec3(-0.5, 0, 0), glm::dvec3(0.5, 0, 0), glm::dvec3(0.0, 0.5, 0)},
        {glm::dvec3(0.0, 0.5, 0), glm::dvec3(0.5, 0, 0), glm::dvec3(0.5, 0.5, 0)}};

    CAPTURE(clipped_soup);
    CAPTURE(expected1);
    CAPTURE(expected2);
    CHECK((clipped_soup == expected1 || clipped_soup == expected2));
}

TEST_CASE("mesh::clip_on_bounds cuts a triangle crossing a corner of the bounds with no vertex inside", "[mesh][clip]") {
    // The part in bounds is made only of cuts, and the x plane leaves vertices that the y plane then removes.
    SimpleMesh mesh;
    mesh.positions = {
        glm::dvec3(-1, 1, 0),
        glm::dvec3(1, -3, 0),
        glm::dvec3(9, -1, 0),
    };
    mesh.triangles = {
        glm::uvec3(0, 1, 2)};
    const radix::geometry::Aabb3d bounds(glm::dvec3(0, 0, -1), glm::dvec3(10, 10, 1));
    const SimpleMesh clipped_mesh = mesh::clip_on_bounds(mesh, bounds).into_owned();
    const TriangleSoup clipped_soup = to_sorted_triangle_soup(clipped_mesh);

    double total_area = 0.0;
    for (const auto &triangle : clipped_soup) {
        const auto &a = triangle[0];
        const auto &b = triangle[1];
        const auto &c = triangle[2];

        CAPTURE(a, b, c);
        CHECK(bounds.contains_inclusive(a));
        CHECK(bounds.contains_inclusive(b));
        CHECK(bounds.contains_inclusive(c));

        total_area += geometry::compute_triangle_area(a, b, c);
    }

    // The clipped region is the triangle (0, 0), (4, 0), (0, 0.8).
    CAPTURE(clipped_soup);
    CHECK(!clipped_soup.empty());
    CHECK_THAT(total_area, WithinEpsilon(1.6));
}

TEST_CASE("mesh::clip_on_bounds keeps coincident cut points of separate triangles apart", "[mesh][clip]") {
    SimpleMesh mesh;
    mesh.positions = {
        // Triangle A
        glm::dvec3(1.0, 0.0, 0.0),
        glm::dvec3(-1.0, 0.0, 0.0),
        glm::dvec3(-1.0, 1.0, 0.0),

        // Triangle B (disconnected, but will intersect at same point)
        glm::dvec3(1.0, 0.0, 0.2),
        glm::dvec3(-1.0, 0.0, -0.2),
        glm::dvec3(-1.0, -1.0, -0.2),
    };

    mesh.triangles = {
        glm::uvec3(0, 1, 2),
        glm::uvec3(3, 4, 5),
    };

    const radix::geometry::Aabb3d bounds(
        glm::dvec3(0.0, -10.0, -10.0),
        glm::dvec3(10.0, 10.0, 10.0));

    const SimpleMesh clipped = mesh::clip_on_bounds(mesh, bounds).into_owned();

    const TriangleSoup clipped_soup = to_sorted_triangle_soup(clipped);
    CHECK(clipped_soup == TriangleSoup{{
                                           glm::dvec3(0.0, -0.5, 0.0),
                                           glm::dvec3(1.0, 0.0, 0.2),
                                           glm::dvec3(0.0, 0.0, 0.0),
                                       },
                                       {
                                           glm::dvec3(0.0, 0.0, 0.0),
                                           glm::dvec3(0.0, 0.5, 0.0),
                                           glm::dvec3(1.0, 0.0, 0.0),
                                       }});
    CHECK(mesh::is_manifold(clipped));
    CHECK(clipped.positions.size() == 6);
}

TEST_CASE("mesh::clip_on_bounds interpolates the uvs of cut points", "[mesh][clip]") {
    // The cut at x = 0.25 lies a quarter along the edge from vertex 0 and three quarters along the edge from vertex 1.
    SimpleMesh mesh;
    mesh.positions = {
        glm::dvec3(0, 0, 0),
        glm::dvec3(1, 0, 0),
        glm::dvec3(0, 1, 0),
    };
    mesh.uvs = {
        glm::dvec2(0, 0),
        glm::dvec2(0, 1),
        glm::dvec2(1, 0),
    };
    mesh.triangles = {
        glm::uvec3(0, 1, 2)};
    const radix::geometry::Aabb3d bounds(glm::dvec3(-1, -1, -1), glm::dvec3(0.25, 2, 1));

    const SimpleMesh clipped_mesh = mesh::clip_on_bounds(mesh, bounds).into_owned();

    const std::vector<std::pair<glm::dvec3, glm::dvec2>> expected_vertices = {
        {glm::dvec3(0, 0, 0), glm::dvec2(0, 0)},
        {glm::dvec3(0, 1, 0), glm::dvec2(1, 0)},
        {glm::dvec3(0.25, 0, 0), glm::dvec2(0, 0.25)},
        {glm::dvec3(0.25, 0.75, 0), glm::dvec2(0.75, 0.25)},
    };
    REQUIRE(clipped_mesh.positions.size() == expected_vertices.size());
    REQUIRE(clipped_mesh.uvs.size() == expected_vertices.size());
    for (const auto &[position, uv] : expected_vertices) {
        CAPTURE(position);
        const auto it = std::ranges::find(clipped_mesh.positions, position);
        REQUIRE(it != clipped_mesh.positions.end());
        CHECK(clipped_mesh.uvs[std::distance(clipped_mesh.positions.begin(), it)] == uv);
    }
}

TEST_CASE("mesh::clip_on_bounds handles an edge crossing a plane by less than epsilon", "[mesh][clip]") {
    // Two vertices lie on the plane x = 0 and the third lies just outside it, so the part in bounds has no area.
    SimpleMesh mesh;
    mesh.positions = {
        glm::dvec3(0, 0, 0),
        glm::dvec3(0, 1, 0),
        glm::dvec3(-5e-13, 0.5, 0),
    };
    mesh.triangles = {
        glm::uvec3(0, 1, 2)};
    const radix::geometry::Aabb3d bounds(glm::dvec3(0, -1, -1), glm::dvec3(1, 2, 1));

    const SimpleMesh clipped_mesh = mesh::clip_on_bounds(mesh, bounds).into_owned();

    CHECK(clipped_mesh.positions.empty());
    CHECK(clipped_mesh.triangles.empty());
}

TEST_CASE("mesh::clip_on_bounds keeps the exact position of a vertex on a plane", "[mesh][clip]") {
    // The edge from (1.1, -1, 0) ends on the plane y = 0 at (0.1, 0, 0), where 1.1 + (0.1 - 1.1) != 0.1.
    SimpleMesh mesh;
    mesh.positions = {
        glm::dvec3(1.1, -1, 0),
        glm::dvec3(0.1, 0, 0),
        glm::dvec3(1.1, 1, 0),
    };
    mesh.triangles = {
        glm::uvec3(0, 1, 2)};
    const radix::geometry::Aabb3d bounds(glm::dvec3(-2, 0, -1), glm::dvec3(2, 2, 1));

    const SimpleMesh clipped_mesh = mesh::clip_on_bounds(mesh, bounds).into_owned();

    const TriangleSoup clipped_soup = to_sorted_triangle_soup(clipped_mesh);
    CHECK(clipped_soup == TriangleSoup{{
                              glm::dvec3(0.1, 0, 0),
                              glm::dvec3(1.1, 1, 0),
                              glm::dvec3(1.1, 0, 0),
                          }});
}

TEST_CASE("mesh::clip_on_bounds cuts an edge crossing two opposite planes at two vertices", "[mesh][clip]") {
    SimpleMesh mesh;
    mesh.positions = {
        glm::dvec3(-0.5, 0, 0),
        glm::dvec3(1.5, 0, 0),
        glm::dvec3(0.5, 1, 0),
    };
    mesh.triangles = {
        glm::uvec3(0, 1, 2)};
    const radix::geometry::Aabb3d bounds(glm::dvec3(0, -1, -1), glm::dvec3(1, 2, 1));

    const SimpleMesh clipped_mesh = mesh::clip_on_bounds(mesh, bounds).into_owned();

    const std::vector<glm::dvec3> expected_positions = {
        glm::dvec3(0, 0, 0),
        glm::dvec3(1, 0, 0),
        glm::dvec3(1, 0.5, 0),
        glm::dvec3(0.5, 1, 0),
        glm::dvec3(0, 0.5, 0),
    };
    CAPTURE(clipped_mesh.positions);
    CHECK(std::ranges::is_permutation(clipped_mesh.positions, expected_positions));
}

TEST_CASE("mesh::clip_on_bounds keeps the sliver next to a vertex just inside the bounds", "[mesh][clip]") {
    // The cut at x = 1 lies within rounding of the vertex just below it, so one piece has almost no area.
    const double just_below_one = std::nextafter(1.0, 0.0);
    SimpleMesh mesh;
    mesh.positions = {
        glm::dvec3(0, 1, 0),
        glm::dvec3(just_below_one, 0, 0),
        glm::dvec3(2, 0, 0),
    };
    mesh.triangles = {
        glm::uvec3(0, 1, 2)};
    const radix::geometry::Aabb3d bounds(glm::dvec3(-1, -1, -1), glm::dvec3(1, 2, 1));

    const SimpleMesh clipped_mesh = mesh::clip_on_bounds(mesh, bounds).into_owned();
    const TriangleSoup clipped_soup = to_sorted_triangle_soup(clipped_mesh);

    const TriangleSoup expected1 = {
        {glm::dvec3(0, 1, 0), glm::dvec3(just_below_one, 0, 0), glm::dvec3(1, 0, 0)},
        {glm::dvec3(0, 1, 0), glm::dvec3(1, 0, 0), glm::dvec3(1, 0.5, 0)}};
    const TriangleSoup expected2 = {
        {glm::dvec3(0, 1, 0), glm::dvec3(just_below_one, 0, 0), glm::dvec3(1, 0.5, 0)},
        {glm::dvec3(just_below_one, 0, 0), glm::dvec3(1, 0, 0), glm::dvec3(1, 0.5, 0)}};

    CAPTURE(clipped_soup);
    CHECK((clipped_soup == expected1 || clipped_soup == expected2));
}
