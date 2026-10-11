#include "../catch2_helpers.h"
#include "../result_helpers.h"
#include "../temp_directory.h"

#include <array>
#include <cmath>
#include <vector>

#include <glm/glm.hpp>

#include "bvh/BatchWriter.h"
#include "bvh/NodeCache.h"
#include "bvh/Storage.h"
#include "bvh/layout.h"
#include "view/Camera.h"
#include "view/CutRule.h"
#include "view/select_cut.h"

TEST_CASE("select_cut selects the leaves in the cut and skips subtrees that hold none", "[view][select_cut]") {
    /*
               root
            /   |    \
         near behind  fine
        / | \
       a  b  c
    */
    const TempDirectory directory("select_cut");
    bvh::Storage nodes = REQUIRE_VALUE(bvh::Storage::create(directory.path()));

    // Reserved but never written, so walking into it fails.
    const bvh::Batch missing = nodes.create_batch();
    const std::array unreadable{bvh::Reference{.batch = missing.index(), .node = 0}, bvh::Reference{.batch = missing.index(), .node = 1}};

    // The leaves lie at a distance of about 10, where an error fits if it is at most 5.
    bvh::BatchWriter writer(nodes);
    const bvh::Reference a = REQUIRE_VALUE(writer.write_leaf({.min_child_error = 4.0, .max_parent_error = 6.0, .bounds = {{10.0, -1.0, -1.0}, {12.0, 1.0, 1.0}}}, dag::Id{octree::Id::root(), 5}));
    const bvh::Reference b = REQUIRE_VALUE(writer.write_leaf({.min_child_error = 2.0, .max_parent_error = 4.0, .bounds = {{10.0, 1.0, -1.0}, {12.0, 3.0, 1.0}}}, dag::Id{octree::Id::root(), 2}));
    const bvh::Reference c = REQUIRE_VALUE(writer.write_leaf({.min_child_error = 4.0, .max_parent_error = 6.0, .bounds = {{10.0, -3.0, -1.0}, {12.0, -1.0, 1.0}}}, dag::Id{octree::Id::root(), 1}));
    const bvh::Reference near = REQUIRE_VALUE(writer.write_inner({.min_child_error = 2.0, .max_parent_error = 6.0, .bounds = {{10.0, -3.0, -1.0}, {12.0, 3.0, 1.0}}}, std::array{a, b, c}));
    // Behind the camera.
    const bvh::Reference behind = REQUIRE_VALUE(writer.write_inner({.min_child_error = 4.0, .max_parent_error = 6.0, .bounds = {{-12.0, -1.0, -1.0}, {-10.0, 1.0, 1.0}}}, unreadable));
    // In view, with errors that all fit.
    const bvh::Reference fine = REQUIRE_VALUE(writer.write_inner({.min_child_error = 0.1, .max_parent_error = 0.2, .bounds = {{10.0, -1.0, 5.0}, {12.0, 1.0, 7.0}}}, unreadable));
    const bvh::Reference root = REQUIRE_VALUE(writer.write_inner({.min_child_error = 0.1, .max_parent_error = 6.0, .bounds = {{-12.0, -3.0, -1.0}, {12.0, 3.0, 7.0}}}, std::array{near, behind, fine}));
    REQUIRE_VALUE(writer.flush());
    REQUIRE_VALUE(nodes.save_index());

    // An error at a distance shows as their ratio, so it fits if it is at most half the distance.
    const view::Camera camera = REQUIRE_VALUE(view::Camera::create({.eye = {0.0, 0.0, 0.0}, .forward = {1.0, 0.0, 0.0}, .up = {0.0, 0.0, 1.0}, .fov_y = 2.0 * std::atan(0.5), .aspect = 1.0, .z_near = 1.0}));
    bvh::NodeCache cache(nodes, 1);
    const view::Selection selection = REQUIRE_VALUE(view::select_cut(cache, root, camera, view::CutRule{.threshold = 0.5}));

    // a and c, sorted. b is left out because its parent's error fits as well.
    CHECK(selection.clusters == std::vector<dag::Id>{{octree::Id::root(), 1}, {octree::Id::root(), 5}});
    // root, near, behind and fine.
    CHECK(selection.inner_nodes_tested == 4);
    // a, b and c.
    CHECK(selection.leaves_tested == 3);
}
