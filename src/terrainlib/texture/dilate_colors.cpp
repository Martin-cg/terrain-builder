#include "texture/dilate_colors.h"

#include <cstdint>
#include <limits>
#include <span>
#include <vector>

#include <glm/glm.hpp>
#include <libassert/assert.hpp>

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

cv::Mat dilate_colors(const cv::Mat &image, const Mask &coverage, const uint32_t radius) {
    cv::Mat dilated = image.clone();
    Mask grown = coverage.clone();
    dilate_colors_inplace(dilated, grown, radius);
    return dilated;
}

} // namespace texture