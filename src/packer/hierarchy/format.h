#pragma once

#include <cstdint>
#include <limits>
#include <vector>

#include <glm/glm.hpp>
#include <libassert/assert.hpp>
#include <radix/geometry.h>

#include "PageId.h"
#include "PointCodec.h"
#include "glm_utils.h"
#include "io/envelope.h"
#include "numeric/checked_cast.h"
#include "quantize.h"
#include "zpp_bits_utils.h"

namespace packer::hierarchy {

using Frame = PointCodec<3, FixedPoint::repr_t>;
using QuantizedBox = radix::geometry::Aabb<3, uint8_t>;

// The frame the children of a node with `box` are quantized in.
[[nodiscard]] inline Frame derive_frame(const GridBox &box) {
    constexpr uint8_t bits = std::numeric_limits<decltype(QuantizedBox::min)::value_type>::digits;
    return fit_codec(box, bits);
}

// The smallest box in `frame`'s coordinates that contains `box`.
// `box` must lie within what `frame` can address.
[[nodiscard]] inline QuantizedBox quantize_box(const Frame &frame, const GridBox &box) {
    DEBUG_ASSERT(glm::all(glm::lessThanEqual(box.min, box.max)));
    DEBUG_ASSERT(glm::all(glm::greaterThanEqual(box.min, frame.anchor)));

    const Frame::Coordinate min = frame.encode<std::round_toward_neg_infinity>(box.min);
    const Frame::Coordinate max = frame.encode<std::round_toward_infinity>(box.max);

    using Coordinate = decltype(QuantizedBox::min)::value_type;
    return {.min = debug_checked_cast<Coordinate>(min), .max = debug_checked_cast<Coordinate>(max)};
}

// The error as a page stores it.
[[nodiscard]] inline float quantize_error(const double error) {
    return static_cast<float>(error);
}

// The box `quantized` stands for in `frame`.
[[nodiscard]] inline GridBox dequantize_box(const Frame &frame, const QuantizedBox &quantized) {
    return {.min = frame.decode(quantized.min), .max = frame.decode(quantized.max)};
}

// Where the content of a child lives.
enum class ChildKind : uint8_t {
    // A node further down in the same page.
    Local = 0,
    // The root of another hierarchy page.
    External = 1,
    // A cluster in a geometry page.
    Leaf = 2,
};

// A root of another hierarchy page.
struct ExternalChild {
    PageId page;
    uint16_t root = 0;

    auto operator<=>(const ExternalChild &) const = default;
};

// A cluster in a geometry page, textured from a texture page.
struct LeafChild {
    PageId page;
    uint16_t cluster = 0;
    PageId texture_page;
    uint16_t texture = 0;

    auto operator<=>(const LeafChild &) const = default;
};

namespace v1 {

struct EncodedChild {
    QuantizedBox box;
    float min_error;
    float max_parent_error;
    ChildKind kind = ChildKind::Local;

    bool operator==(const EncodedChild &) const = default;
};

struct EncodedPage {
    // The pages that references point into.
    std::vector<PageId> pages;
    // The frame the children of each root are quantized in.
    std::vector<Frame> root_frames;
    // The number of children of each node.
    std::vector<uint8_t> child_counts;
    std::vector<EncodedChild> children;

    // References stay raw, since coding them as an index sequence saves less than a byte per child after compression.
    // An index into `pages` for each External or Leaf child.
    std::vector<uint16_t> page_slots;
    // The position within the referenced page for each External or Leaf child.
    std::vector<uint16_t> local_indices;
    // An index into `pages` for the texture page of each Leaf child.
    std::vector<uint16_t> texture_page_slots;
    // The position within the referenced texture page for each Leaf child.
    std::vector<uint16_t> texture_local_indices;

    bool operator==(const EncodedPage &) const = default;
};

}

using EncodedChild = v1::EncodedChild;
using EncodedPage = v1::EncodedPage;
using EncodedPageSchema = io::envelope::PayloadSchema<"HierarchyPage", io::envelope::Version<1, v1::EncodedPage>>;

}