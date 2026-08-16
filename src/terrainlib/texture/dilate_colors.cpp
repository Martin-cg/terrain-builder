#include "texture/dilate_colors.h"

#include <algorithm>
#include <stdexcept>
#include <utility>
#include <vector>

#include <glm/gtx/component_wise.hpp>
#include <radix/geometry.h>

#include "geometry/geometry.h"
#include "opencv_utils.h"

namespace texture {
namespace {

template <int n_channels>
struct Neighbourhood {
    cv::Vec<double, n_channels> sum;
    uint32_t count = 0;

    bool is_empty() const {
        return this->count == 0;
    }
};

template <int n_channels, typename Channel>
Neighbourhood<n_channels> sum_neighbours(const cv::Mat &image, const Mask &coverage, const int x, const int y) {
    using Texel = cv::Vec<Channel, n_channels>;

    Neighbourhood<n_channels> neighbourhood;
    for (int ny = std::max(y - 1, 0); ny <= std::min(y + 1, image.rows - 1); ny++) {
        for (int nx = std::max(x - 1, 0); nx <= std::min(x + 1, image.cols - 1); nx++) {
            if (!coverage.get(nx, ny)) {
                continue;
            }
            const Texel &neighbour = image.at<Texel>(ny, nx);
            for (int channel = 0; channel < n_channels; channel++) {
                neighbourhood.sum[channel] += neighbour[channel];
            }
            neighbourhood.count++;
        }
    }
    return neighbourhood;
}

template <int n_channels, typename Channel>
void dilate_round(cv::Mat &image, const Mask &coverage, Mask &grown) {
    using Texel = cv::Vec<Channel, n_channels>;

    for (int y = 0; y < image.rows; y++) {
        for (int x = 0; x < image.cols; x++) {
            if (coverage.get(x, y)) {
                continue;
            }

            const Neighbourhood<n_channels> neighbourhood =
                sum_neighbours<n_channels, Channel>(image, coverage, x, y);
            if (neighbourhood.is_empty()) {
                continue;
            }

            Texel &texel = image.at<Texel>(y, x);
            for (int channel = 0; channel < n_channels; channel++) {
                texel[channel] = cv::saturate_cast<Channel>(neighbourhood.sum[channel] / neighbourhood.count);
            }
            grown.set(x, y);
        }
    }
}

template <int n_channels, typename Channel>
void dilate_rounds(cv::Mat &image, Mask &coverage, const uint32_t radius) {
    for (uint32_t round = 0; round < radius; round++) {
        Mask grown = coverage.clone();
        dilate_round<n_channels, Channel>(image, coverage, grown);
        coverage = std::move(grown);
    }
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

template <int n_channels, typename Channel>
void fill_hole(cv::Mat &image, const Hole &hole) {
    using Texel = cv::Vec<Channel, n_channels>;

    for (const glm::ivec2 &texel : hole.texels) {
        cv::Vec<double, n_channels> sum;
        double weight_sum = 0.0;

        for (const glm::ivec2 &rim : hole.rim) {
            const glm::dvec2 offset(rim - texel);
            const double weight = 1.0 / glm::dot(offset, offset);
            const Texel &colour = image.at<Texel>(rim.y, rim.x);
            for (int channel = 0; channel < n_channels; channel++) {
                sum[channel] += weight * colour[channel];
            }
            weight_sum += weight;
        }

        Texel &filled = image.at<Texel>(texel.y, texel.x);
        for (int channel = 0; channel < n_channels; channel++) {
            filled[channel] = cv::saturate_cast<Channel>(sum[channel] / weight_sum);
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
    visit_by_depth_and_channels(image.type(), [&]<int n_channels, typename Channel>() {
        for (const Hole &hole : holes) {
            fill_hole<n_channels, Channel>(image, hole);
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

void dilate_colors_inplace(cv::Mat &image, Mask &coverage, const uint32_t radius) {
    if (radius == 0) {
        return;
    }
    if (coverage.size() != glm::uvec2(image.cols, image.rows)) {
        throw std::invalid_argument("dilate_colors_inplace: coverage must have the size of the image");
    }

    visit_by_depth_and_channels(image.type(), [&]<int n_channels, typename Channel>() {
        dilate_rounds<n_channels, Channel>(image, coverage, radius);
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