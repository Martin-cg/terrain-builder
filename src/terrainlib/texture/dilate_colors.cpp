#include "texture/dilate_colors.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtx/component_wise.hpp>
#include <libassert/assert.hpp>
#include <radix/geometry.h>

#include "geometry/geometry.h"
#include "opencv_utils.h"

namespace texture {
namespace {

// Visits the up-to-eight texels surrounding the given one.
template <typename Visitor>
void for_each_neighbour(const glm::uvec2 texel, const glm::uvec2 resolution, Visitor &&visit) {
    for (uint32_t dy = 0; dy < 3; dy++) {
        for (uint32_t dx = 0; dx < 3; dx++) {
            const glm::uvec2 neighbour = texel + glm::uvec2(dx, dy) - glm::uvec2(1);
            if (neighbour == texel) {
                continue;
            }
            // A coordinate below zero wraps high, so one test cuts both edges.
            if (glm::any(glm::greaterThanEqual(neighbour, resolution))) {
                continue;
            }
            visit(neighbour);
        }
    }
}

template <int channels>
struct Neighbourhood {
    cv::Vec<double, channels> sum;
    uint32_t count = 0;
};

template <int channels, typename Value>
Neighbourhood<channels> sum_neighbours(const cv::Mat &image, const Mask &coverage, const glm::uvec2 texel) {
    using Texel = cv::Vec<Value, channels>;

    Neighbourhood<channels> neighbourhood;
    for_each_neighbour(texel, coverage.size(), [&](const glm::uvec2 neighbour) {
        if (!coverage.get(neighbour.x, neighbour.y)) {
            return;
        }
        neighbourhood.sum += convert_texel<double>(image.at<Texel>(neighbour.y, neighbour.x));
        neighbourhood.count++;
    });
    return neighbourhood;
}

// The target texels bordering the covered region, marking them reached.
template <typename InTarget>
std::vector<glm::uvec2> find_border(Mask &reached, const InTarget &in_target, const Mask &coverage) {
    const glm::uvec2 resolution = coverage.size();

    std::vector<glm::uvec2> border;
    for (uint32_t y = 0; y < resolution.y; y++) {
        for (uint32_t x = 0; x < resolution.x; x++) {
            const glm::uvec2 texel(x, y);
            if (coverage.get(x, y) || !in_target(texel)) {
                continue;
            }

            bool touches_covered = false;
            for_each_neighbour(texel, resolution, [&](const glm::uvec2 neighbour) {
                touches_covered = touches_covered || coverage.get(neighbour.x, neighbour.y);
            });
            if (!touches_covered) {
                continue;
            }

            reached.set(x, y);
            border.push_back(texel);
        }
    }
    return border;
}

// The unreached target texels bordering the given ones, marking them reached.
template <typename InTarget>
std::vector<glm::uvec2> find_border(Mask &reached, const InTarget &in_target, const std::span<const glm::uvec2> texels) {
    const glm::uvec2 resolution = reached.size();

    std::vector<glm::uvec2> border;
    for (const glm::uvec2 texel : texels) {
        for_each_neighbour(texel, resolution, [&](const glm::uvec2 neighbour) {
            if (reached.get(neighbour.x, neighbour.y) || !in_target(neighbour)) {
                return;
            }
            reached.set(neighbour.x, neighbour.y);
            border.push_back(neighbour);
        });
    }
    return border;
}

// Fills the given texels with the mean of their covered neighbours, then marks them covered.
template <int channels, typename Value>
void dilate_round(cv::Mat &image, Mask &coverage, const std::span<const glm::uvec2> texels) {
    using Texel = cv::Vec<Value, channels>;

    for (const glm::uvec2 texel : texels) {
        const Neighbourhood<channels> neighbourhood =
            sum_neighbours<channels, Value>(image, coverage, texel);

        image.at<Texel>(texel.y, texel.x) = convert_texel<Value>(neighbourhood.sum / static_cast<double>(neighbourhood.count));
    }

    for (const glm::uvec2 texel : texels) {
        coverage.set(texel.x, texel.y);
    }
}

// Grows the covered region into the target, one border at a time. Returns the texels filled.
template <int channels, typename Value, typename InTarget>
uint32_t dilate_rounds(cv::Mat &image, Mask &coverage, const InTarget &in_target, const std::optional<uint32_t> max_radius) {
    Mask reached = coverage.clone();
    std::vector<glm::uvec2> texels = find_border(reached, in_target, coverage);

    uint32_t dilated = 0;
    uint32_t remaining_rounds = max_radius.value_or(std::numeric_limits<uint32_t>::max());
    while (!texels.empty() && remaining_rounds > 0) {
        dilate_round<channels, Value>(image, coverage, texels);
        dilated += texels.size();
        remaining_rounds--;

        if (remaining_rounds == 0) {
            break;
        }
        texels = find_border(reached, in_target, texels);
    }

    return dilated;
}

// A run of uncovered texels, and the covered texels touching it.
struct Hole {
    std::vector<glm::ivec2> texels;
    std::vector<glm::ivec2> rim;
};

// Marks every texel whose centre falls in a uv triangle but which nothing wrote to.
Mask find_missing_texels(
    const Mask &coverage,
    const std::span<const glm::uvec3> triangles,
    const std::span<const glm::dvec2> uvs) {
    const glm::uvec2 resolution = coverage.size();
    const glm::dvec2 uv_scale(resolution);
    const glm::ivec2 highest(resolution.x - 1, resolution.y - 1);

    Mask missing(resolution);
    for (const glm::uvec3 &triangle : triangles) {
        const radix::geometry::Triangle<2, double> pixel_triangle = {
            uvs[triangle.x] * uv_scale,
            uvs[triangle.y] * uv_scale,
            uvs[triangle.z] * uv_scale};
        const glm::dvec2 lowest_pixel = glm::min(glm::min(pixel_triangle[0], pixel_triangle[1]), pixel_triangle[2]);
        const glm::dvec2 highest_pixel = glm::max(glm::max(pixel_triangle[0], pixel_triangle[1]), pixel_triangle[2]);
        const glm::ivec2 first = glm::max(glm::ivec2(glm::floor(lowest_pixel)), glm::ivec2(0));
        const glm::ivec2 last = glm::min(glm::ivec2(glm::ceil(highest_pixel)), highest);

        for (int32_t y = first.y; y <= last.y; y++) {
            for (int32_t x = first.x; x <= last.x; x++) {
                if (coverage.get(x, y) || missing.get(x, y)) {
                    continue;
                }
                const glm::dvec3 weights = geometry::compute_barycentric(glm::dvec2(x + 0.5, y + 0.5), pixel_triangle);
                if (glm::compMin(weights) < 0.0) {
                    continue;
                }
                missing.set(x, y);
            }
        }
    }

    return missing;
}

// Every missing texel connected to the seed, clearing each one it claims.
std::vector<glm::ivec2> collect_hole(Mask &missing, const glm::ivec2 seed) {
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
std::vector<glm::ivec2> collect_rim(const Mask &coverage, const std::span<const glm::ivec2> texels) {
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
            const Texel &colour = image.at<Texel>(rim.y, rim.x);
            for (int channel = 0; channel < channels; channel++) {
                sum[channel] += weight * colour[channel];
            }
            weight_sum += weight;
        }

        Texel &filled = image.at<Texel>(texel.y, texel.x);
        for (int channel = 0; channel < channels; channel++) {
            filled[channel] = cv::saturate_cast<Value>(sum[channel] / weight_sum);
        }
    }
}

// Every hole in the patch, all read off the coverage the render left before any of them is filled.
std::vector<Hole> find_holes(
    const Mask &coverage,
    const std::span<const glm::uvec3> triangles,
    const std::span<const glm::dvec2> uvs) {
    const glm::uvec2 resolution = coverage.size();
    const glm::ivec2 highest(resolution.x - 1, resolution.y - 1);
    Mask missing = find_missing_texels(coverage, triangles, uvs);

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
uint32_t fill_holes(cv::Mat &image, Mask &coverage, const std::span<const Hole> holes) {
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

} // namespace

uint32_t dilate_colors_inplace(cv::Mat &image, Mask &coverage, const Mask &target, const std::optional<uint32_t> max_radius) {
    ASSERT(coverage.size() == glm::uvec2(image.cols, image.rows));
    ASSERT(target.size() == glm::uvec2(image.cols, image.rows));

    uint32_t dilated = 0;
    visit_by_depth_and_channels(image.type(), [&]<int channels, typename Value>() {
        dilated = dilate_rounds<channels, Value>(
            image, coverage, [&](const glm::uvec2 texel) { return target.get(texel.x, texel.y); }, max_radius);
    });
    return dilated;
}

void dilate_colors_inplace(cv::Mat &image, Mask &coverage, const uint32_t radius) {
    if (radius == 0) {
        return;
    }
    ASSERT(coverage.size() == glm::uvec2(image.cols, image.rows));

    visit_by_depth_and_channels(image.type(), [&]<int channels, typename Value>() {
        dilate_rounds<channels, Value>(image, coverage, [](const glm::uvec2) { return true; }, radius);
    });
}

uint32_t interpolate_uncovered(
    cv::Mat &image,
    Mask &coverage,
    const std::span<const glm::uvec3> triangles,
    const std::span<const glm::dvec2> uvs) {
    if (coverage.size() != glm::uvec2(image.cols, image.rows)) {
        throw std::invalid_argument("interpolate_uncovered: coverage must have the size of the image");
    }

    const std::vector<Hole> holes = find_holes(coverage, triangles, uvs);
    return fill_holes(image, coverage, holes);
}

cv::Mat dilate_colors(const cv::Mat &image, const Mask &coverage, const uint32_t radius) {
    cv::Mat dilated = image.clone();
    Mask grown = coverage.clone();
    dilate_colors_inplace(dilated, grown, radius);
    return dilated;
}

} // namespace texture
