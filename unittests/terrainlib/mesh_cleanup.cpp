#include <cstdint>
#include <span>
#include <vector>

#include "../catch2_helpers.h"
#include "mesh/SimpleMesh.h"
#include "mesh/cleanup.h"

TEST_CASE("mesh::find_duplicate_triangle_groups", "[mesh][cleanup]") {
    const std::vector<glm::uvec3> triangles = {
        glm::uvec3(0, 1, 2), // 0
        glm::uvec3(3, 4, 5), // 1
        glm::uvec3(4, 5, 3), // 2 rotation of 1
        glm::uvec3(1, 2, 0), // 3 rotation of 0
        glm::uvec3(0, 2, 1), // 4 flip of 0
        glm::uvec3(6, 7, 8), // 5 unique
        glm::uvec3(5, 4, 3), // 6 flip of 1
        glm::uvec3(0, 1, 2), // 7 copy of 0
    };

    SECTION("considering orientation, rotations match and flips do not") {
        const SegmentedBuffer<uint32_t, uint32_t> groups = mesh::find_duplicate_triangle_groups(triangles, false);

        SegmentedBuffer<uint32_t, uint32_t> expected;
        expected.push_new_segment(std::vector<uint32_t>{1, 2});
        expected.push_new_segment(std::vector<uint32_t>{0, 3, 7});
        CHECK(groups == expected);
    }

    SECTION("ignoring orientation, rotations and flips match") {
        const SegmentedBuffer<uint32_t, uint32_t> groups = mesh::find_duplicate_triangle_groups(triangles, true);

        SegmentedBuffer<uint32_t, uint32_t> expected;
        expected.push_new_segment(std::vector<uint32_t>{1, 2, 6});
        expected.push_new_segment(std::vector<uint32_t>{0, 3, 4, 7});
        CHECK(groups == expected);
    }
}

TEST_CASE("mesh::find_duplicate_triangle_groups without duplicates is empty", "[mesh][cleanup]") {
    const std::vector<glm::uvec3> triangles = {
        glm::uvec3(0, 1, 2),
        glm::uvec3(1, 3, 2),
    };

    CHECK(mesh::find_duplicate_triangle_groups(triangles, true) == SegmentedBuffer<uint32_t, uint32_t>());
}

TEST_CASE("mesh::find_duplicate_triangles", "[mesh][cleanup]") {
    const std::vector<glm::dvec3> positions = {
        glm::dvec3(0, 0, 0), // 0
        glm::dvec3(1, 0, 0), // 1
        glm::dvec3(0, 1, 0), // 2
        glm::dvec3(1, 1, 0), // 3
    };

    SECTION("a single triangle has no duplicates") {
        const std::vector<glm::uvec3> triangles = {
            glm::uvec3(0, 1, 2),
        };

        CHECK(mesh::find_duplicate_triangles_consider_orientation(triangles).empty());
        CHECK(mesh::find_duplicate_triangles_ignore_orientation(triangles, positions).empty());
    }

    SECTION("an exact copy is a duplicate in both modes and the first is kept") {
        const std::vector<glm::uvec3> triangles = {
            glm::uvec3(0, 1, 2),
            glm::uvec3(0, 1, 2),
        };

        CHECK(mesh::find_duplicate_triangles_consider_orientation(triangles) == std::vector<uint32_t>{1});
        CHECK(mesh::find_duplicate_triangles_ignore_orientation(triangles, positions) == std::vector<uint32_t>{1});
    }

    SECTION("a flipped copy is a duplicate only when orientation is ignored") {
        const std::vector<glm::uvec3> triangles = {
            glm::uvec3(0, 1, 2),
            glm::uvec3(0, 2, 1),
        };

        CHECK(mesh::find_duplicate_triangles_consider_orientation(triangles).empty());
        CHECK(mesh::find_duplicate_triangles_ignore_orientation(triangles, positions) == std::vector<uint32_t>{1});
    }

    SECTION("ignoring orientation, the copy facing like its neighbours is kept") {
        SECTION("neighbour facing +z keeps the first copy") {
            const std::vector<glm::uvec3> triangles = {
                glm::uvec3(0, 1, 2), // +z
                glm::uvec3(0, 2, 1), // -z
                glm::uvec3(1, 3, 2), // +z, shares edge 1-2
            };

            CHECK(mesh::find_duplicate_triangles_ignore_orientation(triangles, positions) == std::vector<uint32_t>{1});
        }

        SECTION("neighbour facing -z keeps the flipped copy") {
            const std::vector<glm::uvec3> triangles = {
                glm::uvec3(0, 1, 2), // +z
                glm::uvec3(0, 2, 1), // -z
                glm::uvec3(2, 3, 1), // -z, shares edge 1-2
            };

            CHECK(mesh::find_duplicate_triangles_ignore_orientation(triangles, positions) == std::vector<uint32_t>{0});
        }
    }

    SECTION("three copies with one flipped keep only the first aligned copy") {
        const std::vector<glm::uvec3> triangles = {
            glm::uvec3(0, 1, 2),
            glm::uvec3(0, 2, 1),
            glm::uvec3(0, 1, 2),
        };

        const std::vector<uint32_t> duplicates = mesh::find_duplicate_triangles_ignore_orientation(triangles, positions);

        CHECK(duplicates == std::vector<uint32_t>{1, 2});
    }
}

TEST_CASE("mesh::remove_duplicate_triangles", "[mesh][cleanup]") {
    const std::vector<glm::dvec3> positions = {
        glm::dvec3(0, 0, 0),
        glm::dvec3(1, 0, 0),
        glm::dvec3(0, 1, 0),
    };

    SECTION("a single triangle is left unchanged in both modes") {
        std::vector<glm::uvec3> triangles = {
            glm::uvec3(0, 1, 2),
        };
        const std::vector<glm::uvec3> old_triangles = triangles;

        mesh::remove_duplicate_triangles(triangles, std::span<const glm::dvec3>(positions), false);
        CHECK(triangles == old_triangles);

        mesh::remove_duplicate_triangles(triangles, std::span<const glm::dvec3>(positions), true);
        CHECK(triangles == old_triangles);
    }

    SECTION("considering orientation, exact copies are removed and flipped ones kept") {
        std::vector<glm::uvec3> triangles = {
            glm::uvec3(0, 1, 2),
            glm::uvec3(0, 1, 2),
            glm::uvec3(0, 2, 1),
        };

        mesh::remove_duplicate_triangles(triangles, std::span<const glm::dvec3>(positions), false);

        CHECK(triangles == std::vector<glm::uvec3>{
            glm::uvec3(0, 1, 2),
            glm::uvec3(0, 2, 1),
        });
    }

    SECTION("ignoring orientation, exact and flipped copies are removed") {
        std::vector<glm::uvec3> triangles = {
            glm::uvec3(0, 1, 2),
            glm::uvec3(0, 1, 2),
            glm::uvec3(0, 2, 1),
        };

        mesh::remove_duplicate_triangles(triangles, std::span<const glm::dvec3>(positions), true);

        CHECK(triangles == std::vector<glm::uvec3>{
            glm::uvec3(0, 1, 2),
        });
    }
}

TEST_CASE("mesh::remove_unused_vertices with listed vertices", "[mesh][cleanup]") {
    SECTION("an empty list leaves the mesh unchanged") {
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
            glm::uvec3(0, 1, 2),
        };
        const std::vector<glm::dvec3> old_positions = mesh.positions;
        const std::vector<glm::dvec2> old_uvs = mesh.uvs;
        const std::vector<glm::uvec3> old_triangles = mesh.triangles;

        mesh::remove_unused_vertices(mesh, std::vector<uint32_t>{});

        CHECK(mesh.positions == old_positions);
        CHECK(mesh.uvs == old_uvs);
        CHECK(mesh.triangles == old_triangles);
    }

    SECTION("removing the last vertices leaves the triangles unchanged") {
        SimpleMesh mesh;
        mesh.positions = {
            glm::dvec3(0, 0, 0), // 0
            glm::dvec3(1, 0, 0), // 1
            glm::dvec3(0, 1, 0), // 2
            glm::dvec3(5, 5, 5), // 3 removed
            glm::dvec3(6, 6, 6), // 4 removed
        };
        mesh.uvs = {
            glm::dvec2(0, 0),
            glm::dvec2(1, 0),
            glm::dvec2(0, 1),
            glm::dvec2(5, 5),
            glm::dvec2(6, 6),
        };
        mesh.triangles = {
            glm::uvec3(0, 1, 2),
        };
        const std::vector<glm::uvec3> old_triangles = mesh.triangles;

        mesh::remove_unused_vertices(mesh, std::vector<uint32_t>{3, 4});

        CHECK(mesh.positions == std::vector<glm::dvec3>{
            glm::dvec3(0, 0, 0),
            glm::dvec3(1, 0, 0),
            glm::dvec3(0, 1, 0),
        });
        CHECK(mesh.uvs == std::vector<glm::dvec2>{
            glm::dvec2(0, 0),
            glm::dvec2(1, 0),
            glm::dvec2(0, 1),
        });
        CHECK(mesh.triangles == old_triangles);
    }

    SECTION("removing every vertex of a mesh without triangles empties it") {
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

        mesh::remove_unused_vertices(mesh, std::vector<uint32_t>{0, 1, 2});

        CHECK(mesh.positions.empty());
        CHECK(mesh.uvs.empty());
        CHECK(mesh.triangles.empty());
    }

    SECTION("removing vertices in the middle keeps the rest in order and renumbers the triangles") {
        SimpleMesh mesh;
        mesh.positions = {
            glm::dvec3(0, 0, 0), // 0
            glm::dvec3(5, 5, 5), // 1 removed
            glm::dvec3(1, 0, 0), // 2
            glm::dvec3(6, 6, 6), // 3 removed
            glm::dvec3(0, 1, 0), // 4
            glm::dvec3(1, 1, 0), // 5
        };
        mesh.uvs = {
            glm::dvec2(0, 0),
            glm::dvec2(5, 5),
            glm::dvec2(1, 0),
            glm::dvec2(6, 6),
            glm::dvec2(0, 1),
            glm::dvec2(1, 1),
        };
        mesh.triangles = {
            glm::uvec3(0, 2, 4),
            glm::uvec3(2, 5, 4),
        };

        mesh::remove_unused_vertices(mesh, std::vector<uint32_t>{1, 3});

        CHECK(mesh.positions == std::vector<glm::dvec3>{
            glm::dvec3(0, 0, 0),
            glm::dvec3(1, 0, 0),
            glm::dvec3(0, 1, 0),
            glm::dvec3(1, 1, 0),
        });
        CHECK(mesh.uvs == std::vector<glm::dvec2>{
            glm::dvec2(0, 0),
            glm::dvec2(1, 0),
            glm::dvec2(0, 1),
            glm::dvec2(1, 1),
        });
        CHECK(mesh.triangles == std::vector<glm::uvec3>{
            glm::uvec3(0, 1, 2),
            glm::uvec3(1, 3, 2),
        });
    }
}

TEST_CASE("mesh::remove_unused_vertices", "[mesh][cleanup]") {
    SECTION("unused vertices in the middle are removed and the rest keep their order") {
        SimpleMesh mesh;
        mesh.positions = {
            glm::dvec3(0, 0, 0), // 0
            glm::dvec3(5, 5, 5), // 1 unused
            glm::dvec3(1, 0, 0), // 2
            glm::dvec3(6, 6, 6), // 3 unused
            glm::dvec3(0, 1, 0), // 4
            glm::dvec3(1, 1, 0), // 5
        };
        mesh.uvs = {
            glm::dvec2(0, 0),
            glm::dvec2(5, 5),
            glm::dvec2(1, 0),
            glm::dvec2(6, 6),
            glm::dvec2(0, 1),
            glm::dvec2(1, 1),
        };
        mesh.triangles = {
            glm::uvec3(0, 2, 4),
            glm::uvec3(2, 5, 4),
        };

        const size_t removed = mesh::remove_unused_vertices(mesh);

        CHECK(removed == 2);
        CHECK(mesh.positions == std::vector<glm::dvec3>{
            glm::dvec3(0, 0, 0),
            glm::dvec3(1, 0, 0),
            glm::dvec3(0, 1, 0),
            glm::dvec3(1, 1, 0),
        });
        CHECK(mesh.uvs == std::vector<glm::dvec2>{
            glm::dvec2(0, 0),
            glm::dvec2(1, 0),
            glm::dvec2(0, 1),
            glm::dvec2(1, 1),
        });
        CHECK(mesh.triangles == std::vector<glm::uvec3>{
            glm::uvec3(0, 1, 2),
            glm::uvec3(1, 3, 2),
        });
    }
}

TEST_CASE("mesh::remove_degenerate_triangles", "[mesh][cleanup]") {
    SECTION("removes triangles with a repeated vertex in any position") {
        std::vector<glm::uvec3> triangles = {
            glm::uvec3(0, 1, 2), // valid
            glm::uvec3(0, 0, 1), // first equals second
            glm::uvec3(2, 3, 3), // second equals third
            glm::uvec3(1, 2, 1), // third equals first
            glm::uvec3(1, 3, 2), // valid
        };

        mesh::remove_degenerate_triangles(triangles);

        CHECK(triangles == std::vector<glm::uvec3>{
            glm::uvec3(0, 1, 2),
            glm::uvec3(1, 3, 2),
        });
    }

    SECTION("triangles without a repeated vertex stay unchanged") {
        std::vector<glm::uvec3> triangles = {
            glm::uvec3(0, 1, 2),
            glm::uvec3(1, 3, 2),
        };
        const std::vector<glm::uvec3> old_triangles = triangles;

        mesh::remove_degenerate_triangles(triangles);

        CHECK(triangles == old_triangles);
    }
}

TEST_CASE("mesh::remove_triangles_of_negligible_size", "[mesh][cleanup]") {
    // Areas 32, 2 and 0.5, so the average is 11.5.
    SimpleMesh mesh;
    mesh.positions = {
        glm::dvec3(0, 0, 0), // 0
        glm::dvec3(8, 0, 0), // 1
        glm::dvec3(0, 8, 0), // 2
        glm::dvec3(0, 0, 1), // 3
        glm::dvec3(2, 0, 1), // 4
        glm::dvec3(0, 2, 1), // 5
        glm::dvec3(0, 0, 2), // 6
        glm::dvec3(1, 0, 2), // 7
        glm::dvec3(0, 1, 2), // 8
    };
    mesh.triangles = {
        glm::uvec3(0, 1, 2), // area 32
        glm::uvec3(3, 4, 5), // area 2
        glm::uvec3(6, 7, 8), // area 0.5
    };

    SECTION("removes triangles below the fraction of the average area") {
        // The cutoff is 1.15, between the two smaller areas.
        const size_t removed = mesh::remove_triangles_of_negligible_size(mesh, 0.1);

        CHECK(removed == 1);
        CHECK(mesh.triangles == std::vector<glm::uvec3>{
            glm::uvec3(0, 1, 2),
            glm::uvec3(3, 4, 5),
        });
    }

    SECTION("threshold zero removes nothing") {
        const std::vector<glm::uvec3> old_triangles = mesh.triangles;

        const size_t removed = mesh::remove_triangles_of_negligible_size(mesh, 0.0);

        CHECK(removed == 0);
        CHECK(mesh.triangles == old_triangles);
    }

    SECTION("float threshold on a double mesh") {
        // The cutoff is 5.75.
        const size_t removed = mesh::remove_triangles_of_negligible_size(mesh, 0.5f);

        CHECK(removed == 2);
        CHECK(mesh.triangles == std::vector<glm::uvec3>{
            glm::uvec3(0, 1, 2),
        });
    }
}

TEST_CASE("mesh::remove_triangles_of_negligible_size keeps triangles exactly at the cutoff", "[mesh][cleanup]") {
    // Both areas are 0.5, which equals the cutoff for a fraction of 1.
    SimpleMesh mesh;
    mesh.positions = {
        glm::dvec3(0, 0, 0),
        glm::dvec3(1, 0, 0),
        glm::dvec3(0, 1, 0),
        glm::dvec3(1, 1, 0),
    };
    mesh.triangles = {
        glm::uvec3(0, 1, 2),
        glm::uvec3(1, 3, 2),
    };
    const std::vector<glm::uvec3> old_triangles = mesh.triangles;

    const size_t removed = mesh::remove_triangles_of_negligible_size(mesh, 1.0);

    CHECK(removed == 0);
    CHECK(mesh.triangles == old_triangles);
}
