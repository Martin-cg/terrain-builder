#include <cstdint>
#include <vector>

#include "../catch2_helpers.h"
#include "containers/SegmentedBuffer.h"
#include "mesh/connectivity/adjacency.h"

TEST_CASE("mesh::find_edge_neighbourhoods", "[mesh][connectivity][adjacency]") {
    const std::vector<glm::uvec3> triangles = {
        glm::uvec3(0, 1, 2), // 0
        glm::uvec3(1, 3, 2), // 1 shares 1-2 with 0
        glm::uvec3(3, 4, 2), // 2 shares 2-3 with 1, only vertex 2 with 0
        glm::uvec3(0, 2, 1), // 3 flip of 0, shares 1-2 with 1
        glm::uvec3(5, 6, 7), // 4 unrelated
        glm::uvec3(2, 1, 8), // 5 third triangle on 1-2
    };
    const std::vector<uint32_t> queried = {1, 0};

    const SegmentedBuffer<uint32_t, uint32_t> neighbourhoods = mesh::find_edge_neighbourhoods(triangles, queried);

    SegmentedBuffer<uint32_t, uint32_t> expected;
    expected.push_new_segment(std::vector<uint32_t>{0, 1, 2, 3, 5});
    expected.push_new_segment(std::vector<uint32_t>{0, 1, 3, 5});
    CHECK(neighbourhoods == expected);
}

TEST_CASE("mesh::find_edge_neighbourhoods without queried triangles is empty", "[mesh][connectivity][adjacency]") {
    const std::vector<glm::uvec3> triangles = {
        glm::uvec3(0, 1, 2),
    };

    CHECK(mesh::find_edge_neighbourhoods(triangles, {}) == SegmentedBuffer<uint32_t, uint32_t>());
}
