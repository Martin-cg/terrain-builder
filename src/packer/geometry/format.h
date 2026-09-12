#pragma once

#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "io/envelope.h"
#include "numeric/quantize.h"
#include "quantize.h"

namespace packer::geometry {

using QuantizedUv = glm::u16vec2;

[[nodiscard]] inline QuantizedUv quantize_uv(const glm::dvec2 &uv) {
    const glm::dvec2 clamped = glm::clamp(uv, 0.0, 1.0);
    return quantize_normalized<QuantizedUv::value_type>(clamped);
}

[[nodiscard]] inline glm::dvec2 dequantize_uv(const QuantizedUv quantized) {
    return dequantize_normalized<double>(quantized);
}

namespace v1 {

struct EncodedClusterHeader {
    uint32_t vertex_count = 0;
    uint32_t triangle_count = 0;
    uint16_t encoded_meshlet_byte_count = 0;

    auto operator<=>(const EncodedClusterHeader &) const = default;
};

struct EncodedPage {
    std::vector<EncodedClusterHeader> clusters;
    uint32_t position_count = 0;
    std::vector<uint8_t> positions;
    std::vector<uint8_t> uvs;
    std::vector<uint8_t> meshlets;

    auto operator<=>(const EncodedPage &) const = default;
};

}

using EncodedClusterHeader = v1::EncodedClusterHeader;
using EncodedPage = v1::EncodedPage;
using EncodedPageSchema = io::envelope::PayloadSchema<"GeometryPage", io::envelope::Version<1, v1::EncodedPage>>;

struct ClusterHeader {
    uint32_t vertex_count = 0;
    uint32_t triangle_count = 0;

    auto operator<=>(const ClusterHeader &) const = default;
};

// TODO: maybe store each distinct (position, UV) pair once, which wins where clusters share one UV space.
// A geometry page with every stream decoded.
struct Page {
    std::vector<ClusterHeader> clusters;
    std::vector<QuantizedPoint> positions;
    std::vector<uint32_t> position_indices;
    std::vector<QuantizedUv> uvs;
    std::vector<glm::u8vec3> triangles;

    bool operator==(const Page &) const = default;
};

}