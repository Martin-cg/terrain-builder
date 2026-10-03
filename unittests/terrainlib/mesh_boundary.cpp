#include <algorithm>
#include <cstdint>
#include <unordered_set>
#include <vector>

#include "../catch2_helpers.h"

#include "mesh/SimpleMesh.h"
#include "mesh/connectivity/boundary.h"

TEST_CASE("mesh::find_boundary_edges", "[mesh][connectivity][boundary]") {
    SECTION("empty for empty mesh") {
        SimpleMesh mesh;
        const auto edges = mesh::find_boundary_edges(mesh);

        const std::vector<glm::uvec2> actual(edges.begin(), edges.end());
        const std::vector<glm::uvec2> expected;

        CHECK_THAT(actual, Catch::Matchers::UnorderedEquals(expected));
    }

    SECTION("all edges for single triangle") {
        SimpleMesh mesh;
        mesh.positions = {
            {0, 0, 0},
            {1, 0, 0},
            {0, 1, 0}};
        mesh.triangles = {glm::uvec3(0, 1, 2)};

        const auto edges = mesh::find_boundary_edges(mesh);

        const std::vector<glm::uvec2> actual(edges.begin(), edges.end());
        const std::vector<glm::uvec2> expected = {
            {0, 1}, {1, 2}, {2, 0}};
        CHECK_THAT(actual, Catch::Matchers::UnorderedEquals(expected));
    }

    SECTION("two triangles sharing an edge") {
        SimpleMesh mesh;
        mesh.positions = {
            {0, 0, 0},
            {1, 0, 0},
            {0, 1, 0},
            {1, 1, 0}};
        mesh.triangles = {
            {0, 1, 2},
            {1, 3, 2}};

        const auto edges = mesh::find_boundary_edges(mesh);

        const std::vector<glm::uvec2> actual(edges.begin(), edges.end());
        const std::vector<glm::uvec2> expected = {
            {0, 1}, {2, 0}, {1, 3}, {3, 2} // shared edge {1,2} not included
        };

        CHECK_THAT(actual, Catch::Matchers::UnorderedEquals(expected));
    }

    SECTION("empty for cube mesh") {
        SimpleMesh mesh;
        mesh.positions = {
            {-1, -1, -1}, // 0
            {1, -1, -1},  // 1
            {1, 1, -1},   // 2
            {-1, 1, -1},  // 3
            {-1, -1, 1},  // 4
            {1, -1, 1},   // 5
            {1, 1, 1},    // 6
            {-1, 1, 1}    // 7
        };

        mesh.triangles = {
            {0, 1, 2}, {0, 2, 3}, // front
            {1, 5, 6}, {1, 6, 2}, // right
            {5, 4, 7}, {5, 7, 6}, // back
            {4, 0, 3}, {4, 3, 7}, // left
            {3, 2, 6}, {3, 6, 7}, // top
            {4, 5, 1}, {4, 1, 0}  // bottom
        };

        const auto edges = mesh::find_boundary_edges(mesh);

        const std::vector<glm::uvec2> actual(edges.begin(), edges.end());
        const std::vector<glm::uvec2> expected;

        CHECK_THAT(actual, Catch::Matchers::UnorderedEquals(expected));
    }

    SECTION("all edges for bowtie sharing vertex") {
        const std::vector<glm::uvec3> triangles = {
            {0, 1, 2},
            {1, 3, 4}};

        const auto edges = mesh::find_boundary_edges(triangles);

        const std::vector<glm::uvec2> actual(edges.begin(), edges.end());
        const std::vector<glm::uvec2> expected = {
            {0, 1}, {1, 2}, {2, 0}, {1, 3}, {3, 4}, {4, 1}};
        CHECK_THAT(actual, Catch::Matchers::UnorderedEquals(expected));
    }

    SECTION("three triangles sharing an edge") {
        const std::vector<glm::uvec3> triangles = {
            {0, 1, 2},
            {1, 0, 3},
            {0, 1, 4}};

        const auto edges = mesh::find_boundary_edges(triangles);

        const std::vector<glm::uvec2> actual(edges.begin(), edges.end());
        const std::vector<glm::uvec2> expected = {
            {1, 2}, {2, 0}, {0, 3}, {3, 1}, {1, 4}, {4, 0} // shared edge {0,1} not included
        };
        CHECK_THAT(actual, Catch::Matchers::UnorderedEquals(expected));
    }

    SECTION("three triangles sharing an edge in a different order") {
        const std::vector<glm::uvec3> triangles = {
            {0, 1, 2},
            {0, 1, 4},
            {1, 0, 3}};

        const auto edges = mesh::find_boundary_edges(triangles);

        const std::vector<glm::uvec2> actual(edges.begin(), edges.end());
        const std::vector<glm::uvec2> expected = {
            {1, 2}, {2, 0}, {0, 3}, {3, 1}, {1, 4}, {4, 0} // shared edge {0,1} not included
        };
        CHECK_THAT(actual, Catch::Matchers::UnorderedEquals(expected));
    }

    SECTION("two triangles sharing an edge in the same direction") {
        const std::vector<glm::uvec3> triangles = {
            {0, 1, 2},
            {0, 1, 3}};

        const auto edges = mesh::find_boundary_edges(triangles);

        const std::vector<glm::uvec2> actual(edges.begin(), edges.end());
        const std::vector<glm::uvec2> expected = {
            {1, 2}, {2, 0}, {1, 3}, {3, 0} // shared edge {0,1} not included
        };
        CHECK_THAT(actual, Catch::Matchers::UnorderedEquals(expected));
    }

    SECTION("empty for two tetrahedra sharing an edge") {
        const std::vector<glm::uvec3> triangles = {
            {0, 2, 1}, {0, 1, 3}, {0, 3, 2}, {1, 2, 3},
            {0, 4, 1}, {0, 1, 5}, {0, 5, 4}, {1, 4, 5}};

        const auto edges = mesh::find_boundary_edges(triangles);

        const std::vector<glm::uvec2> actual(edges.begin(), edges.end());
        const std::vector<glm::uvec2> expected;
        CHECK_THAT(actual, Catch::Matchers::UnorderedEquals(expected));
    }
}

TEST_CASE("mesh::find_boundary_vertices", "[mesh][connectivity][boundary]") {
    SECTION("outer vertices of a closed fan") {
        const std::vector<glm::uvec3> triangles = {
            {0, 1, 4}, {1, 2, 4}, {2, 3, 4}, {3, 0, 4}};

        const std::unordered_set<uint32_t> actual = mesh::find_boundary_vertices(triangles);

        const std::unordered_set<uint32_t> expected = {0, 1, 2, 3};
        CHECK(actual == expected);
    }

    SECTION("replaces the contents of the output set") {
        const std::vector<glm::uvec3> triangles = {
            {0, 1, 4}, {1, 2, 4}, {2, 3, 4}, {3, 0, 4}};
        std::unordered_set<uint32_t> actual = {4, 9};

        mesh::find_boundary_vertices(triangles, actual);

        const std::unordered_set<uint32_t> expected = {0, 1, 2, 3};
        CHECK(actual == expected);
    }

    SECTION("all vertices for bowtie sharing vertex") {
        const std::vector<glm::uvec3> triangles = {
            {0, 1, 2},
            {1, 3, 4}};

        const std::unordered_set<uint32_t> actual = mesh::find_boundary_vertices(triangles);

        const std::unordered_set<uint32_t> expected = {0, 1, 2, 3, 4};
        CHECK(actual == expected);
    }

    SECTION("all vertices for three triangles sharing an edge") {
        const std::vector<glm::uvec3> triangles = {
            {0, 1, 2},
            {1, 0, 3},
            {0, 1, 4}};

        const std::unordered_set<uint32_t> actual = mesh::find_boundary_vertices(triangles);

        const std::unordered_set<uint32_t> expected = {0, 1, 2, 3, 4};
        CHECK(actual == expected);
    }

    SECTION("empty for two tetrahedra sharing an edge") {
        const std::vector<glm::uvec3> triangles = {
            {0, 2, 1}, {0, 1, 3}, {0, 3, 2}, {1, 2, 3},
            {0, 4, 1}, {0, 1, 5}, {0, 5, 4}, {1, 4, 5}};

        const std::unordered_set<uint32_t> actual = mesh::find_boundary_vertices(triangles);

        CHECK(actual.empty());
    }
}

TEST_CASE("mesh::find_boundary_triangles", "[mesh][connectivity][boundary]") {
    SECTION("sides of an open box") {
        // 5 faces of a cube (top removed)
        const std::vector<glm::uvec3> triangles = {
            {0, 1, 2}, {0, 2, 3},   // front
            {1, 5, 6}, {1, 6, 2},   // right
            {5, 4, 7}, {5, 7, 6},   // back
            {4, 0, 3}, {4, 3, 7},   // left
            {4, 5, 1}, {4, 1, 0},   // bottom
        };

        const std::unordered_set<uint32_t> actual = mesh::find_boundary_triangles(triangles);

        const std::unordered_set<uint32_t> expected = {1, 3, 5, 7};
        CHECK(actual == expected);
    }

    SECTION("replaces the contents of the output set") {
        const std::vector<glm::uvec3> triangles = {
            {0, 1, 2}, {0, 2, 3},   // front
            {1, 5, 6}, {1, 6, 2},   // right
            {5, 4, 7}, {5, 7, 6},   // back
            {4, 0, 3}, {4, 3, 7},   // left
            {4, 5, 1}, {4, 1, 0},   // bottom
        };
        std::unordered_set<uint32_t> actual = {0, 42};

        mesh::find_boundary_triangles(triangles, actual);

        const std::unordered_set<uint32_t> expected = {1, 3, 5, 7};
        CHECK(actual == expected);
    }

    SECTION("both triangles of a bowtie sharing vertex") {
        const std::vector<glm::uvec3> triangles = {
            {0, 1, 2},
            {1, 3, 4}};

        const std::unordered_set<uint32_t> actual = mesh::find_boundary_triangles(triangles);

        const std::unordered_set<uint32_t> expected = {0, 1};
        CHECK(actual == expected);
    }

    SECTION("all triangles sharing an edge") {
        const std::vector<glm::uvec3> triangles = {
            {0, 1, 2},
            {1, 0, 3},
            {0, 1, 4}};

        const std::unordered_set<uint32_t> actual = mesh::find_boundary_triangles(triangles);

        const std::unordered_set<uint32_t> expected = {0, 1, 2};
        CHECK(actual == expected);
    }

    SECTION("empty for two tetrahedra sharing an edge") {
        const std::vector<glm::uvec3> triangles = {
            {0, 2, 1}, {0, 1, 3}, {0, 3, 2}, {1, 2, 3},
            {0, 4, 1}, {0, 1, 5}, {0, 5, 4}, {1, 4, 5}};

        const std::unordered_set<uint32_t> actual = mesh::find_boundary_triangles(triangles);

        CHECK(actual.empty());
    }
}

namespace {

void normalize_boundary(std::vector<uint32_t> &boundary) {
    if (boundary.empty()) {
        return;
    }
    auto min_it = std::min_element(boundary.begin(), boundary.end());
    std::rotate(boundary.begin(), min_it, boundary.end());
}

void normalize_boundaries(std::vector<std::vector<uint32_t>> &boundaries) {
    for (auto &b : boundaries) {
        normalize_boundary(b);
    }
    std::sort(boundaries.begin(), boundaries.end());
}

}

TEST_CASE("mesh::find_boundaries", "[mesh][connectivity][boundary]") {
    SECTION("empty for empty mesh") {
        SimpleMesh mesh;
        auto actual = mesh::find_boundaries(mesh);
        normalize_boundaries(actual);
        const std::vector<std::vector<uint32_t>> expected = {};
        CHECK(actual == expected);
    }

    SECTION("single triangle") {
        SimpleMesh mesh;
        mesh.positions = {
            {0, 0, 0},
            {1, 0, 0},
            {0, 1, 0}};
        mesh.triangles = {glm::uvec3(0, 1, 2)};

        auto actual = mesh::find_boundaries(mesh);
        normalize_boundaries(actual);
        const std::vector<std::vector<uint32_t>> expected = {{0, 1, 2}};
        CHECK(actual == expected);
    }

    SECTION("two triangles sharing an edge") {
        SimpleMesh mesh;
        mesh.positions = {
            {0, 0, 0},
            {1, 0, 0},
            {0, 1, 0},
            {1, 1, 0}};
        mesh.triangles = {
            {0, 1, 2},
            {1, 3, 2}};

        auto actual = mesh::find_boundaries(mesh);
        normalize_boundaries(actual);
        const std::vector<std::vector<uint32_t>> expected = {{0, 1, 3, 2}};
        CHECK(actual == expected);
    }

    SECTION("two separate triangles") {
        SimpleMesh mesh;
        mesh.positions = {
            {0, 0, 0}, // 0
            {1, 0, 0}, // 1
            {0, 1, 0}, // 2
            {2, 0, 0}, // 3
            {3, 0, 0}, // 4
            {2, 1, 0}  // 5
        };
        mesh.triangles = {
            {0, 1, 2},
            {3, 4, 5}};

        auto actual = mesh::find_boundaries(mesh);
        normalize_boundaries(actual);
        const std::vector<std::vector<uint32_t>> expected = {
            {0, 1, 2},
            {3, 4, 5}
        };
        CHECK(actual == expected);
    }

    SECTION("square with triangular hole") {
        SimpleMesh mesh;
        mesh.positions = {
            {0, 0, 0}, // 0
            {1, 0, 0}, // 1
            {1, 1, 0}, // 2
            {0, 1, 0}, // 3
            {0.25f, 0.25f, 0}, // 4
            {0.75f, 0.25f, 0}, // 5
            {0.5f, 0.75f, 0}   // 6
        };
        mesh.triangles = {
            {0, 1, 4}, {1, 5, 4}, {1, 2, 5}, {2, 6, 5}, {2, 3, 6}, {3, 4, 6}, {3, 0, 4}
        };

        auto actual = mesh::find_boundaries(mesh);
        normalize_boundaries(actual);
        const std::vector<std::vector<uint32_t>> expected = {
            {0, 1, 2, 3},
            {4, 6, 5},
        };
        CHECK(actual == expected);
    }

    SECTION("empty for cube mesh") {
        SimpleMesh mesh;
        mesh.positions = {
            {-1, -1, -1}, // 0
            {1, -1, -1},  // 1
            {1, 1, -1},   // 2
            {-1, 1, -1},  // 3
            {-1, -1, 1},  // 4
            {1, -1, 1},   // 5
            {1, 1, 1},    // 6
            {-1, 1, 1}    // 7
        };

        mesh.triangles = {
            {0, 1, 2}, {0, 2, 3}, // front
            {1, 5, 6}, {1, 6, 2}, // right
            {5, 4, 7}, {5, 7, 6}, // back
            {4, 0, 3}, {4, 3, 7}, // left
            {3, 2, 6}, {3, 6, 7}, // top
            {4, 5, 1}, {4, 1, 0}  // bottom
        };

        auto actual = mesh::find_boundaries(mesh);
        normalize_boundaries(actual);
        const std::vector<std::vector<uint32_t>> expected = {};
        CHECK(actual == expected);
    }
}

TEST_CASE("mesh::find_boundaries_non_manifold", "[mesh][connectivity][boundary]") {
    SECTION("empty for empty mesh") {
        SimpleMesh mesh;
        auto actual = mesh::find_boundaries_non_manifold(mesh);
        normalize_boundaries(actual);
        const std::vector<std::vector<uint32_t>> expected = {};
        CHECK(actual == expected);
    }

    SECTION("single triangle") {
        SimpleMesh mesh;
        mesh.positions = {
            {0, 0, 0},
            {1, 0, 0},
            {0, 1, 0}};
        mesh.triangles = {glm::uvec3(0, 1, 2)};

        auto actual = mesh::find_boundaries_non_manifold(mesh);
        normalize_boundaries(actual);
        const std::vector<std::vector<uint32_t>> expected = {{0, 1, 2}};
        CHECK(actual == expected);
    }

    SECTION("two triangles sharing an edge") {
        SimpleMesh mesh;
        mesh.positions = {
            {0, 0, 0},
            {1, 0, 0},
            {0, 1, 0},
            {1, 1, 0}};
        mesh.triangles = {
            {0, 1, 2},
            {1, 3, 2}};

        auto actual = mesh::find_boundaries_non_manifold(mesh);
        normalize_boundaries(actual);
        const std::vector<std::vector<uint32_t>> expected = {{0, 1, 3, 2}};
        CHECK(actual == expected);
    }

    SECTION("two separate triangles") {
        SimpleMesh mesh;
        mesh.positions = {
            {0, 0, 0}, // 0
            {1, 0, 0}, // 1
            {0, 1, 0}, // 2
            {2, 0, 0}, // 3
            {3, 0, 0}, // 4
            {2, 1, 0}  // 5
        };
        mesh.triangles = {
            {0, 1, 2},
            {3, 4, 5}};

        auto actual = mesh::find_boundaries_non_manifold(mesh);
        normalize_boundaries(actual);
        const std::vector<std::vector<uint32_t>> expected = {
            {0, 1, 2},
            {3, 4, 5}};
        CHECK(actual == expected);
    }

    SECTION("empty for cube mesh") {
        SimpleMesh mesh;
        mesh.positions = {
            {-1, -1, -1}, // 0
            {1, -1, -1},  // 1
            {1, 1, -1},   // 2
            {-1, 1, -1},  // 3
            {-1, -1, 1},  // 4
            {1, -1, 1},   // 5
            {1, 1, 1},    // 6
            {-1, 1, 1}    // 7
        };

        mesh.triangles = {
            {0, 1, 2}, {0, 2, 3}, // front
            {1, 5, 6},
            {1, 6, 2}, // right
            {5, 4, 7},
            {5, 7, 6}, // back
            {4, 0, 3},
            {4, 3, 7}, // left
            {3, 2, 6},
            {3, 6, 7}, // top
            {4, 5, 1},
            {4, 1, 0} // bottom
        };

        auto actual = mesh::find_boundaries_non_manifold(mesh);
        normalize_boundaries(actual);
        const std::vector<std::vector<uint32_t>> expected = {};
        CHECK(actual == expected);
    }

    SECTION("bowtie sharing vertex") {
        const std::vector<glm::uvec3> triangles = {
            {0, 1, 2},
            {1, 3, 4},
        };

        auto actual = mesh::find_boundaries_non_manifold(triangles);
        normalize_boundaries(actual);

        const std::vector<std::vector<uint32_t>> expected = {
            {0, 1, 2},
            {1, 3, 4},
        };

        CHECK(actual == expected);
    }

    SECTION("three triangles sharing vertex") {
        const std::vector<glm::uvec3> triangles = {
            {0, 1, 2},
            {0, 3, 4},
            {0, 5, 6}
        };

        auto actual = mesh::find_boundaries_non_manifold(triangles);
        normalize_boundaries(actual);

        const std::vector<std::vector<uint32_t>> expected = {
            {0, 1, 2},
            {0, 3, 4},
            {0, 5, 6}
        };

        CHECK(actual == expected);
    }

    SECTION("two quads glued along a diagonal") {
        // v = 0, u = 1, a = 2, b = 3, c = 4, d = 5
        const std::vector<glm::uvec3> triangles = {
            {0, 2, 1}, {1, 3, 0},
            {0, 4, 1}, {1, 5, 0}};

        auto actual = mesh::find_boundaries_non_manifold(triangles);
        normalize_boundaries(actual);

        const std::vector<std::vector<uint32_t>> loops1 = {{0, 2, 1, 3}, {0, 4, 1, 5}};
        const std::vector<std::vector<uint32_t>> loops2 = {{0, 2, 1, 5}, {0, 4, 1, 3}};
        CHECK_THAT(actual, Catch::Matchers::Equals(loops1) || Catch::Matchers::Equals(loops2));
    }

    SECTION("two quads glued along a diagonal in a different order") {
        // v = 0, u = 1, a = 2, b = 3, c = 4, d = 5
        const std::vector<glm::uvec3> triangles = {
            {1, 5, 0}, {0, 4, 1},
            {1, 3, 0}, {0, 2, 1}};

        auto actual = mesh::find_boundaries_non_manifold(triangles);
        normalize_boundaries(actual);

        const std::vector<std::vector<uint32_t>> loops1 = {{0, 2, 1, 3}, {0, 4, 1, 5}};
        const std::vector<std::vector<uint32_t>> loops2 = {{0, 2, 1, 5}, {0, 4, 1, 3}};
        CHECK_THAT(actual, Catch::Matchers::Equals(loops1) || Catch::Matchers::Equals(loops2));
    }
}

TEST_CASE("mesh::build_boundary_vertex_mask", "[mesh][connectivity][boundary]") {
    SECTION("marks the outer vertices of a closed fan and leaves the rest") {
        const std::vector<glm::uvec3> triangles = {
            {0, 1, 4}, {1, 2, 4}, {2, 3, 4}, {3, 0, 4}};

        std::vector<uint8_t> mask;
        mesh::build_boundary_vertex_mask<uint8_t>(triangles, 6, mask, 7, 3);

        const std::vector<uint8_t> expected = {7, 7, 7, 7, 3, 3};
        CHECK(mask == expected);
    }

    SECTION("returns one for boundary vertices and zero otherwise") {
        const std::vector<glm::uvec3> triangles = {
            {0, 1, 4}, {1, 2, 4}, {2, 3, 4}, {3, 0, 4}};

        const std::vector<uint8_t> mask = mesh::build_boundary_vertex_mask(triangles, 6);

        const std::vector<uint8_t> expected = {1, 1, 1, 1, 0, 0};
        CHECK(mask == expected);
    }

    SECTION("all vertices for bowtie sharing vertex") {
        const std::vector<glm::uvec3> triangles = {
            {0, 1, 2},
            {1, 3, 4}};

        std::vector<uint8_t> mask;
        mesh::build_boundary_vertex_mask<uint8_t>(triangles, 5, mask, 7, 3);

        const std::vector<uint8_t> expected = {7, 7, 7, 7, 7};
        CHECK(mask == expected);
    }

    SECTION("no vertices for two tetrahedra sharing an edge") {
        const std::vector<glm::uvec3> triangles = {
            {0, 2, 1}, {0, 1, 3}, {0, 3, 2}, {1, 2, 3},
            {0, 4, 1}, {0, 1, 5}, {0, 5, 4}, {1, 4, 5}};

        std::vector<uint8_t> mask;
        mesh::build_boundary_vertex_mask<uint8_t>(triangles, 6, mask, 7, 3);

        const std::vector<uint8_t> expected = {3, 3, 3, 3, 3, 3};
        CHECK(mask == expected);
    }
}

TEST_CASE("mesh::build_boundary_triangle_mask", "[mesh][connectivity][boundary]") {
    SECTION("open box with more triangles than vertices") {
        // 5 faces of a cube (top removed): 8 vertices, 10 triangles
        const std::vector<glm::uvec3> triangles = {
            {0, 1, 2}, {0, 2, 3},   // front
            {1, 5, 6}, {1, 6, 2},   // right
            {5, 4, 7}, {5, 7, 6},   // back
            {4, 0, 3}, {4, 3, 7},   // left
            {4, 5, 1}, {4, 1, 0},   // bottom
        };

        std::vector<uint8_t> mask;
        mesh::build_boundary_triangle_mask<uint8_t>(triangles, mask, 7, 3);

        const std::vector<uint8_t> expected = {3, 7, 3, 7, 3, 7, 3, 7, 3, 3};
        CHECK(mask == expected);
    }

    SECTION("returns one for boundary triangles and zero otherwise") {
        // 5 faces of a cube (top removed)
        const std::vector<glm::uvec3> triangles = {
            {0, 1, 2}, {0, 2, 3},   // front
            {1, 5, 6}, {1, 6, 2},   // right
            {5, 4, 7}, {5, 7, 6},   // back
            {4, 0, 3}, {4, 3, 7},   // left
            {4, 5, 1}, {4, 1, 0},   // bottom
        };

        const std::vector<uint8_t> mask = mesh::build_boundary_triangle_mask(triangles);

        const std::vector<uint8_t> expected = {0, 1, 0, 1, 0, 1, 0, 1, 0, 0};
        CHECK(mask == expected);
    }

    SECTION("both triangles of a bowtie sharing vertex") {
        const std::vector<glm::uvec3> triangles = {
            {0, 1, 2},
            {1, 3, 4}};

        std::vector<uint8_t> mask;
        mesh::build_boundary_triangle_mask<uint8_t>(triangles, mask, 7, 3);

        const std::vector<uint8_t> expected = {7, 7};
        CHECK(mask == expected);
    }

    SECTION("all triangles sharing an edge") {
        const std::vector<glm::uvec3> triangles = {
            {0, 1, 2},
            {1, 0, 3},
            {0, 1, 4}};

        std::vector<uint8_t> mask;
        mesh::build_boundary_triangle_mask<uint8_t>(triangles, mask, 7, 3);

        const std::vector<uint8_t> expected = {7, 7, 7};
        CHECK(mask == expected);
    }

    SECTION("no triangles for two tetrahedra sharing an edge") {
        const std::vector<glm::uvec3> triangles = {
            {0, 2, 1}, {0, 1, 3}, {0, 3, 2}, {1, 2, 3},
            {0, 4, 1}, {0, 1, 5}, {0, 5, 4}, {1, 4, 5}};

        std::vector<uint8_t> mask;
        mesh::build_boundary_triangle_mask<uint8_t>(triangles, mask, 7, 3);

        const std::vector<uint8_t> expected = {3, 3, 3, 3, 3, 3, 3, 3};
        CHECK(mask == expected);
    }
}
