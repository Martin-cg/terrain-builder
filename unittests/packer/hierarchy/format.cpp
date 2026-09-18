#include <cstdint>
#include <vector>

#include "../../catch2_helpers.h"
#include "hierarchy/format.h"

namespace packer {

TEST_CASE("hierarchy::quantize_box contains the box it quantizes", "[hierarchy][format]") {
    struct Case {
        GridBox parent;
        std::vector<GridBox> children;
    };

    const std::vector<Case> cases = {
        // A step of 1, so every corner lies on a coordinate.
        {.parent = {.min = {0, 0, 0}, .max = {255, 255, 255}},
         .children = {
             {.min = {0, 0, 0}, .max = {255, 255, 255}},
             {.min = {200, 0, 17}, .max = {255, 255, 18}},
         }},
        // A step of 2, so odd corners lie between coordinates.
        {.parent = {.min = {0, 0, 0}, .max = {256, 256, 256}},
         .children = {
             {.min = {0, 0, 0}, .max = {256, 256, 256}},
             {.min = {85, 85, 85}, .max = {171, 171, 171}},
             {.min = {255, 0, 1}, .max = {256, 1, 1}},
         }},
        // A negative anchor, a flat axis and different steps per axis.
        {.parent = {.min = {-1000, 7, 1048576}, .max = {-1, 7, 1056353}},
         .children = {
             {.min = {-1000, 7, 1048576}, .max = {-1, 7, 1056353}},
             {.min = {-999, 7, 1048577}, .max = {-998, 7, 1048577}},
             {.min = {-3, 7, 1056000}, .max = {-1, 7, 1056353}},
         }},
    };

    for (const Case &c : cases) {
        const hierarchy::Frame frame = hierarchy::derive_frame(c.parent);
        for (const GridBox &child : c.children) {
            const GridBox decoded = hierarchy::dequantize_box(frame, hierarchy::quantize_box(frame, child));
            REQUIRE(decoded.contains_inclusive(child.min));
            REQUIRE(decoded.contains_inclusive(child.max));
        }
    }
}

TEST_CASE("hierarchy::quantize_box rejects a box reaching past its frame", "[hierarchy][format]") {
    const hierarchy::Frame frame = hierarchy::derive_frame({.min = {0, 0, 0}, .max = {255, 255, 255}});

    CHECK_THROWS(hierarchy::quantize_box(frame, {.min = {0, 0, 0}, .max = {1000, 0, 0}}));
}

}
