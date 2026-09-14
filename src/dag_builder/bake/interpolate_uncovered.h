#pragma once

#include <algorithm>
#include <cstdint>
#include <ranges>
#include <span>
#include <vector>

#include <glm/glm.hpp>
#include <libassert/assert.hpp>
#include <opencv2/core.hpp>

#include "bake/UvTriangle.h"
#include "opencv_utils.h"
#include "rasterize.h"
#include "texture/Mask.h"

namespace bake {
namespace detail {

// A run of uncovered texels, and the covered texels touching it.
struct Hole {
    std::vector<glm::ivec2> texels;
    std::vector<glm::ivec2> rim;
};

// Marks every texel whose centre falls in a uv triangle but which nothing wrote to.
texture::Mask find_missing_texels(
    const texture::Mask &coverage,
    std::ranges::input_range auto &&triangles) {
    const glm::uvec2 resolution = coverage.size();
    const glm::dvec2 scale(resolution);

    texture::Mask missing(resolution);
    for (const UvTriangle &triangle : triangles) {
        const raster::FixedTriangle texel_triangle =
            raster::make_triangle(triangle[0] * scale, triangle[1] * scale, triangle[2] * scale);

        raster::scan_triangle(texel_triangle, resolution, [&](const glm::uvec2 texel) {
            if (coverage.get(texel.x, texel.y)) {
                return;
            }
            missing.set(texel.x, texel.y);
        });
    }

    return missing;
}

// Every missing texel connected to the seed, clearing each one it claims.
inline std::vector<glm::ivec2> collect_hole(texture::Mask &missing, const glm::ivec2 seed) {
    const glm::uvec2 resolution = missing.size();
    const glm::ivec2 highest(resolution.x - 1, resolution.y - 1);

    std::vector<glm::ivec2> texels;
    std::vector<glm::ivec2> pending = {seed};
    missing.set(seed.x, seed.y, false);

    while (!pending.empty()) {
        const glm::ivec2 current = pending.back();
        pending.pop_back();
        texels.push_back(current);

        for (int32_t dy = -1; dy <= 1; dy++) {
            for (int32_t dx = -1; dx <= 1; dx++) {
                const glm::ivec2 neighbour = current + glm::ivec2(dx, dy);
                if (glm::any(glm::lessThan(neighbour, glm::ivec2(0))) || glm::any(glm::greaterThan(neighbour, highest))) {
                    continue;
                }
                if (!missing.get(neighbour.x, neighbour.y)) {
                    continue;
                }
                missing.set(neighbour.x, neighbour.y, false);
                pending.push_back(neighbour);
            }
        }
    }

    return texels;
}

// The covered texels touching the hole, each named once.
inline std::vector<glm::ivec2> collect_rim(const texture::Mask &coverage, const std::span<const glm::ivec2> texels) {
    const glm::uvec2 resolution = coverage.size();
    const glm::ivec2 highest(resolution.x - 1, resolution.y - 1);

    std::vector<glm::ivec2> rim;
    for (const glm::ivec2 &texel : texels) {
        for (int32_t dy = -1; dy <= 1; dy++) {
            for (int32_t dx = -1; dx <= 1; dx++) {
                const glm::ivec2 neighbour = texel + glm::ivec2(dx, dy);
                if (glm::any(glm::lessThan(neighbour, glm::ivec2(0))) || glm::any(glm::greaterThan(neighbour, highest))) {
                    continue;
                }
                if (coverage.get(neighbour.x, neighbour.y)) {
                    rim.push_back(neighbour);
                }
            }
        }
    }

    std::ranges::sort(rim, {}, [](const glm::ivec2 t) { return std::pair(t.y, t.x); });
    const auto duplicates = std::ranges::unique(rim);
    rim.erase(duplicates.begin(), duplicates.end());
    return rim;
}

template <int channels, typename Value>
void fill_hole(cv::Mat &image, const Hole &hole) {
    using Texel = cv::Vec<Value, channels>;

    for (const glm::ivec2 &texel : hole.texels) {
        cv::Vec<double, channels> sum;
        double weight_sum = 0.0;

        for (const glm::ivec2 &rim : hole.rim) {
            const glm::dvec2 offset(rim - texel);
            const double weight = 1.0 / glm::dot(offset, offset);
            sum += weight * convert_texel<double>(image.at<Texel>(rim.y, rim.x));
            weight_sum += weight;
        }

        image.at<Texel>(texel.y, texel.x) = convert_texel<Value>(sum / weight_sum);
    }
}

// Every hole in the patch, all read off the coverage the render left before any of them is filled.
std::vector<Hole> find_holes(
    const texture::Mask &coverage,
    std::ranges::input_range auto &&triangles) {
    const glm::uvec2 resolution = coverage.size();
    const glm::ivec2 highest(resolution.x - 1, resolution.y - 1);
    texture::Mask missing = find_missing_texels(coverage, triangles);

    std::vector<Hole> holes;
    for (int32_t y = 0; y <= highest.y; y++) {
        for (int32_t x = 0; x <= highest.x; x++) {
            if (!missing.get(x, y)) {
                continue;
            }

            Hole hole;
            hole.texels = collect_hole(missing, glm::ivec2(x, y));
            hole.rim = collect_rim(coverage, hole.texels);
            if (!hole.rim.empty()) {
                holes.push_back(std::move(hole));
            }
        }
    }

    return holes;
}

// Paints each hole from its rim and marks it covered. Returns the texels filled.
inline uint32_t fill_holes(cv::Mat &image, texture::Mask &coverage, const std::span<const Hole> holes) {
    visit_by_depth_and_channels(image.type(), [&]<int channels, typename Value>() {
        for (const Hole &hole : holes) {
            fill_hole<channels, Value>(image, hole);
        }
    });

    uint32_t filled = 0;
    for (const Hole &hole : holes) {
        for (const glm::ivec2 &texel : hole.texels) {
            coverage.set(texel.x, texel.y);
        }
        filled += hole.texels.size();
    }

    return filled;
}

} // namespace detail

// Fills the uncovered texels inside the given uv triangles, each from the covered texels around
// its hole, weighted by inverse square distance. Returns the texels filled.
uint32_t interpolate_uncovered(
    cv::Mat &image,
    texture::Mask &coverage,
    std::ranges::input_range auto &&triangles) {
    ASSERT(coverage.size() == glm::uvec2(image.cols, image.rows));

    const std::vector<detail::Hole> holes = detail::find_holes(coverage, triangles);
    return detail::fill_holes(image, coverage, holes);
}

} // namespace bake
