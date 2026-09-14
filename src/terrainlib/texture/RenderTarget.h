#pragma once

#include <glm/glm.hpp>
#include <libassert/assert.hpp>
#include <opencv2/core.hpp>

#include "texture/Mask.h"

namespace texture {

// The image a render writes into, with the mask recording which texels it wrote.
struct RenderTarget {
    cv::Mat &image;
    Mask &coverage;

    RenderTarget(cv::Mat &image, Mask &coverage)
        : image(image),
          coverage(coverage) {
        ASSERT(coverage.size() == glm::uvec2(image.cols, image.rows));
    }

    glm::uvec2 size() const {
        return {this->image.cols, this->image.rows};
    }
};

} // namespace texture