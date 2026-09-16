#include <vector>

#include <glm/glm.hpp>

#include "../catch2_helpers.h"

#include "texture_sizing.h"

TEST_CASE("compute_target_size returns the demanded texels when the uvs cover everything", "[texture_sizing]") {
    CHECK(compute_target_size({.texels = 10000, .utilization = 1.0}, 1.0) == glm::uvec2(100, 100));
}

TEST_CASE("compute_target_size grows the texture when the uvs cover only part of it", "[texture_sizing]") {
    // Half the coverage needs twice the area, so 20000 texels.
    CHECK(compute_target_size({.texels = 10000, .utilization = 0.5}, 1.0) == glm::uvec2(142, 142));
}

TEST_CASE("compute_target_size shapes the texture by the demanded aspect", "[texture_sizing]") {
    CHECK(compute_target_size({.texels = 10000, .utilization = 1.0}, 4.0) == glm::uvec2(200, 50));
}

TEST_CASE("compute_target_size caps the growth of a barely covered texture", "[texture_sizing]") {
    const glm::uvec2 clamped = compute_target_size({.texels = 10000, .utilization = 0.1}, 1.0);

    CHECK(compute_target_size({.texels = 10000, .utilization = 0.01}, 1.0) == clamped);
    CHECK(compute_target_size({.texels = 10000, .utilization = 0.0}, 1.0) == clamped);
}

namespace {

BakePlan make_plan(const glm::uvec2 size) {
    return BakePlan{.clusters = {}, .size = size};
}

} // namespace

TEST_CASE("rescale_to_fit_budget leaves sizes alone below the budget", "[texture_sizing]") {
    std::vector<BakePlan> textures{make_plan({100, 100}), make_plan({200, 200})};

    rescale_to_fit_budget(textures, 100000);

    CHECK(textures[0].size == glm::uvec2(100, 100));
    CHECK(textures[1].size == glm::uvec2(200, 200));
}

TEST_CASE("rescale_to_fit_budget scales every size by the same factor", "[texture_sizing]") {
    std::vector<BakePlan> textures{make_plan({100, 100}), make_plan({200, 200})};

    // Half the area of the requested 50000 texels, so every side scales by 0.5.
    rescale_to_fit_budget(textures, 12500);

    CHECK(textures[0].size == glm::uvec2(50, 50));
    CHECK(textures[1].size == glm::uvec2(100, 100));
}

TEST_CASE("rescale_to_fit_budget keeps the aspect of a non square size", "[texture_sizing]") {
    std::vector<BakePlan> textures{make_plan({400, 100})};

    rescale_to_fit_budget(textures, 10000);

    CHECK(textures[0].size == glm::uvec2(200, 50));
}

TEST_CASE("rescale_to_fit_budget floors sizes at one texel", "[texture_sizing]") {
    std::vector<BakePlan> textures{make_plan({64, 64})};

    rescale_to_fit_budget(textures, 1);

    CHECK(textures[0].size == glm::uvec2(1, 1));
}

// Rounding up after the uniform scale can leave the total above the budget. This is
// accepted: max_node_texels is a soft cap, not a guarantee.
TEST_CASE("rescale_to_fit_budget can overshoot the budget on many small sizes", "[texture_sizing]") {
    std::vector<BakePlan> textures(10, make_plan({3, 3}));

    rescale_to_fit_budget(textures, 45);

    CHECK(textures[0].size == glm::uvec2(3, 3));
}

TEST_CASE("clamp_to_max_side leaves sizes within the cap alone", "[texture_sizing]") {
    std::vector<BakePlan> textures{make_plan({100, 50}), make_plan({64, 64})};

    clamp_to_max_side(textures, 100);

    CHECK(textures[0].size == glm::uvec2(100, 50));
    CHECK(textures[1].size == glm::uvec2(64, 64));
}

TEST_CASE("clamp_to_max_side scales the longer side down to the cap and keeps the aspect", "[texture_sizing]") {
    std::vector<BakePlan> textures{make_plan({400, 100}), make_plan({50, 200})};

    clamp_to_max_side(textures, 100);

    CHECK(textures[0].size == glm::uvec2(100, 25));
    CHECK(textures[1].size == glm::uvec2(25, 100));
}

TEST_CASE("clamp_to_max_side floors the shorter side of a strip at one texel", "[texture_sizing]") {
    std::vector<BakePlan> textures{make_plan({76017, 2})};

    clamp_to_max_side(textures, 8192);

    CHECK(textures[0].size == glm::uvec2(8192, 1));
}
