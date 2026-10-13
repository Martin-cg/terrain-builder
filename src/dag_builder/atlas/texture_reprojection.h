#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <span>
#include <vector>

#include <glm/glm.hpp>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

#include "enumerate.h"
#include "fixed_point.h"
#include "mesh/WindingOrder.h"
#include "opencv_utils.h"
#include "rasterize.h"
#include "texture/Interpolation.h"
#include "texture/Mask.h"
#include "texture/RenderTarget.h"
#include "texture/sample_image.h"
#include "range_utils.h"
#include "containers/Vector2D.h"

struct ReprojectionOptions {
    uint32_t sample_scale = 2;
    texture::Interpolation interpolation = texture::Interpolation::Linear;
};

struct MappedTriangle {
    uint32_t source_image_index = 0;
    std::array<glm::dvec2, 3> source_uvs;
    std::array<glm::dvec2, 3> target_uvs;
};

struct RenderedTexture {
    cv::Mat image;
    texture::Mask coverage;

    operator cv::Mat() const {
        return this->image;
    }
};

namespace detail {

using raster::FixedCoord;
using raster::FixedPoint;
using raster::FixedScalar;
using raster::FixedTriangle;

struct PreparedTriangle {
    const cv::Mat *source_image = nullptr;
    FixedTriangle source_pixels;
    FixedTriangle target_samples;
    FixedScalar target_area2{};
};

inline glm::dvec2 clamp_uv(const glm::dvec2 &uv) {
    return glm::clamp(uv, glm::dvec2(0.0), glm::dvec2(1.0));
}

// Source coordinates are in source-pixel space.
// Target coordinates are in supersample-grid space.
inline std::vector<PreparedTriangle> prepare_triangles(
    const std::span<const cv::Mat> source_images,
    const std::span<const MappedTriangle> triangles,
    const glm::uvec2 output_size,
    const uint32_t sample_scale) {
    std::vector<PreparedTriangle> prepared;
    prepared.reserve(triangles.size());

    const glm::dvec2 target_sample_size = glm::dvec2(output_size) * static_cast<double>(sample_scale);

    for (const MappedTriangle &triangle : triangles) {
        const cv::Mat& source_image = source_images[triangle.source_image_index];
        const glm::dvec2 source_size(source_image.cols, source_image.rows);

        PreparedTriangle prepared_triangle;
        prepared_triangle.source_image = &source_image;

        for (const uint8_t k : range<uint8_t>(3)) {
            const glm::dvec2 source_uv = clamp_uv(triangle.source_uvs[k]);
            const glm::dvec2 target_uv = clamp_uv(triangle.target_uvs[k]);

            prepared_triangle.source_pixels[k] = FixedPoint::from_real(source_uv * source_size);
            prepared_triangle.target_samples[k] = FixedPoint::from_real(target_uv * target_sample_size);
        }

        prepared_triangle.target_area2 = fp::orient(
            prepared_triangle.target_samples[0],
            prepared_triangle.target_samples[1],
            prepared_triangle.target_samples[2]);

        // Ignore empty triangles.
        if (prepared_triangle.target_area2 == FixedScalar::zero()) {
            continue;
        }

        // Fix incorrect winding order. xatlas does not enforce consistent uv winding, so
        // roughly half of an atlas' charts arrive mirrored. Swapping both corner sets keeps
        // the uv to source mapping intact.
        if (prepared_triangle.target_area2 < FixedScalar::zero()) {
            prepared_triangle.target_samples = swap_orientation(prepared_triangle.target_samples);
            prepared_triangle.source_pixels = swap_orientation(prepared_triangle.source_pixels);
            prepared_triangle.target_area2 = -prepared_triangle.target_area2;
        }

        prepared.push_back(prepared_triangle);
    }

    return prepared;
}

inline Vector2D<int32_t> build_triangle_index_map(const std::span<const PreparedTriangle> triangles, const glm::uvec2 grid_size) {
    Vector2D<int32_t> triangle_index_map(grid_size.y, grid_size.x, -1);

    for (const auto [triangle_index, triangle] : enumerate<int32_t>(triangles)) {
        raster::scan_triangle(triangle.target_samples, grid_size, [&](const glm::uvec2 sample) {
            triangle_index_map(sample.y, sample.x) = triangle_index;
        });
    }

    return triangle_index_map;
}

inline glm::dvec2 target_to_source(const PreparedTriangle &triangle, const FixedPoint target_sample) {
    // Barycentric weights from oriented sub-triangle areas.
    const FixedScalar w0 = (triangle.target_samples[1] - target_sample).cross(triangle.target_samples[2] - target_sample) / triangle.target_area2;
    const FixedScalar w1 = (triangle.target_samples[2] - target_sample).cross(triangle.target_samples[0] - target_sample) / triangle.target_area2;
    // Compute w2 by subtraction to guarantee w0 + w1 + w2 == FixedScalar::one() exactly.
    const FixedScalar w2 = FixedScalar::one() - w0 - w1;

    const FixedPoint source = triangle.source_pixels[0] * w0 +
                              triangle.source_pixels[1] * w1 +
                              triangle.source_pixels[2] * w2;

    return source.real();
}

template <int channels, typename Value>
void render_samples(
    const texture::RenderTarget target,
    const std::vector<PreparedTriangle> &prepared_triangles,
    const Vector2D<int32_t> &triangle_index_map,
    const ReprojectionOptions &options) {
    const glm::uvec2 output_size = target.size();

    for (uint32_t y = 0; y < output_size.y; y++) {
        for (uint32_t x = 0; x < output_size.x; x++) {
            cv::Vec<float, channels> color = cv::Vec<float, channels>::all(0.0f);
            uint32_t sample_count = 0;

            for (uint32_t sy = 0; sy < options.sample_scale; sy++) {
                for (uint32_t sx = 0; sx < options.sample_scale; sx++) {
                    const glm::uvec2 sample(
                        x * options.sample_scale + sx,
                        y * options.sample_scale + sy);

                    const int32_t triangle_id = triangle_index_map(sample.y, sample.x);
                    if (triangle_id < 0) {
                        continue;
                    }

                    const PreparedTriangle &triangle = prepared_triangles[triangle_id];
                    const FixedPoint target_sample = raster::get_cell_center(sample);
                    const glm::dvec2 source_sample = target_to_source(triangle, target_sample);

                    // Offset by 0.5 to convert from pixel-center to OpenCV texel-index space.
                    color += texture::sample_image<channels, Value>(
                        *triangle.source_image,
                        source_sample - glm::dvec2(0.5),
                        options.interpolation);

                    sample_count++;
                }
            }

            // Dividing by sample_count (not sample_scale^2) correctly averages partially covered pixels.
            if (sample_count > 0) {
                const cv::Vec<float, channels> averaged = color / static_cast<float>(sample_count);
                target.image.at<cv::Vec<Value, channels>>(y, x) = convert_texel<Value>(averaged);
                target.coverage.set(x, y);
            }
        }
    }
}

// Renders every texel the triangles cover, into a target of any type.
inline void render_by_type(
    const texture::RenderTarget target,
    const std::vector<PreparedTriangle> &prepared_triangles,
    const Vector2D<int32_t> &triangle_index_map,
    const ReprojectionOptions &options) {
    visit_by_depth_and_channels(target.image.type(), [&]<int channels, typename Value>() {
        render_samples<channels, Value>(target, prepared_triangles, triangle_index_map, options);
    });
}

} // namespace detail

// Renders the triangles into the given target, writing only the texels they cover.
inline void reproject_into(
    const std::span<const cv::Mat> source_images,
    const std::span<const MappedTriangle> triangles,
    const ReprojectionOptions &options,
    const texture::RenderTarget target) {
    ASSERT(std::ranges::all_of(source_images, [&](const cv::Mat &m) { return m.type() == target.image.type(); }));

    const glm::uvec2 output_size = target.size();
    const std::vector<detail::PreparedTriangle> prepared_triangles =
        detail::prepare_triangles(source_images, triangles, output_size, options.sample_scale);

    const glm::uvec2 grid_size = output_size * options.sample_scale;
    const Vector2D<int32_t> triangle_index_map =
        detail::build_triangle_index_map(prepared_triangles, grid_size);

    detail::render_by_type(target, prepared_triangles, triangle_index_map, options);
}

// Renders the triangles into a new image, cleared to the given colour where nothing covers it.
[[nodiscard]]
inline RenderedTexture reproject(
    const std::span<const cv::Mat> source_images,
    const std::span<const MappedTriangle> triangles,
    const glm::uvec2 output_size,
    const int output_type,
    const cv::Scalar &clear_color,
    const ReprojectionOptions &options) {
    cv::Mat output(output_size.y, output_size.x, output_type, clear_color);
    texture::Mask coverage(output_size);
    reproject_into(source_images, triangles, options, texture::RenderTarget(output, coverage));

    return RenderedTexture{std::move(output), std::move(coverage)};
}
