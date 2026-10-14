#include "../catch2_helpers.h"
#include "../result_helpers.h"
#include "../temp_directory.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <set>
#include <utility>
#include <vector>

#include <glm/glm.hpp>

#include "../dag_builder/dag_node.h"
#include "../dag_builder/encoded.h"
#include "../dag_builder/storage.h"
#include "dag/traverse_dag.h"
#include "octree/storage/open.h"

namespace {

constexpr size_t AMPLE_RESIDENT_BYTES = 1024 * 1024;

// A dag folder holding exactly the given nodes.
void write_dag(const std::filesystem::path &path, std::vector<std::pair<octree::Id, dag::NodeMetadata>> nodes) {
    auto dag = REQUIRE_VALUE(octree::open_folder_indexed<dag::ClusterBatch>(path));
    for (auto &[id, metadata] : nodes) {
        REQUIRE(dag.save(id, dag::ClusterBatch{std::move(metadata), Clustering{}}).has_value());
    }
    REQUIRE(dag.save_index().has_value());
}

// Every group the walk over the dag at `path` visits.
std::vector<dag::GroupInfo> collect_groups(const std::filesystem::path &path, const size_t max_resident_bytes) {
    const auto dag = REQUIRE_VALUE(octree::open_folder_indexed<dag::NodeMetadata>(path));
    std::vector<dag::GroupInfo> visited;
    const auto walked = dag::traverse_groups(dag, max_resident_bytes, [&](const dag::GroupView &group) {
        visited.push_back(group.to_owned());
    });
    REQUIRE(walked.value_or(false));
    return visited;
}

}

TEST_CASE("traverse_groups visits every group once", "[dag][traverse_dag]") {
    const octree::Id root = octree::Id::root();
    const octree::Id child = root.children().value()[0];
    const radix::geometry::Aabb3d bounds{glm::dvec3{0.0}, glm::dvec3{1.0}};

    const TempDirectory directory("traverse_dag");
    write_dag(directory.path(), {
        {root, dag::NodeMetadata{
            .clusters = {{.group_index = 0, .bounds = bounds}, {.group_index = 1, .bounds = bounds}},
            .groups = {
                {.children = {dag::Id{child, 0}}, .error = 1.0, .bounds = bounds},
                {.children = {}, .error = 0.0, .bounds = bounds},
            },
        }},
        {child, dag::NodeMetadata{
            .clusters = {{.group_index = 0, .bounds = bounds}},
            .groups = {{.children = {}, .error = 0.0, .bounds = bounds}},
        }},
    });

    const auto dag = REQUIRE_VALUE(octree::open_folder_indexed<dag::NodeMetadata>(directory.path()));

    std::set<dag::GroupId> visited;
    const auto walked = dag::traverse_groups(dag, AMPLE_RESIDENT_BYTES, [&](const dag::GroupView &group) {
        const auto [entry, inserted] = visited.insert(group.id);
        REQUIRE(inserted);
    });

    REQUIRE(walked.value_or(false));
    CHECK(visited == std::set<dag::GroupId>{{root, 0}, {root, 1}, {child, 0}});
}

TEST_CASE("a group's child clusters have their own bounds and the error and bounds of the group that made them", "[dag][traverse_dag]") {
    // Small enough for reading one child's node to evict the node of the group listing it.
    const size_t max_resident_bytes = GENERATE(AMPLE_RESIDENT_BYTES, size_t{1});

    const octree::Id group_node = octree::Id::root();
    const octree::Id first_child_node = group_node.child(0).value();
    const octree::Id second_child_node = group_node.child(1).value();

    const radix::geometry::Aabb3d group_bounds{glm::dvec3{0.0}, glm::dvec3{1.0}};
    const radix::geometry::Aabb3d first_child_bounds{glm::dvec3{2.0}, glm::dvec3{3.0}};
    const radix::geometry::Aabb3d first_child_group_bounds{glm::dvec3{4.0}, glm::dvec3{5.0}};
    const radix::geometry::Aabb3d second_child_bounds{glm::dvec3{6.0}, glm::dvec3{7.0}};

    const TempDirectory directory("traverse_dag");
    write_dag(directory.path(), {
        {group_node, dag::NodeMetadata{
            .clusters = {{.group_index = 0, .bounds = group_bounds}},
            .groups = {{.children = {dag::Id{first_child_node, 0}, dag::Id{second_child_node, 0}}, .error = 3.0, .bounds = group_bounds}},
        }},
        // The child's own bounds differ from its group's, so the two cannot be mixed up.
        {first_child_node, dag::NodeMetadata{
            .clusters = {{.group_index = 0, .bounds = first_child_bounds}},
            .groups = {{.children = {}, .error = 2.0, .bounds = first_child_group_bounds}},
        }},
        {second_child_node, dag::NodeMetadata{
            .clusters = {{.group_index = 0, .bounds = second_child_bounds}},
            .groups = {{.children = {}, .error = 0.0, .bounds = second_child_bounds}},
        }},
    });

    const std::vector<dag::GroupInfo> groups = collect_groups(directory.path(), max_resident_bytes);
    const auto group_it = std::ranges::find(groups, dag::GroupId{group_node, 0}, &dag::GroupInfo::id);
    REQUIRE(group_it != groups.end());

    CHECK_THAT(group_it->child_clusters,Catch::Matchers::UnorderedEquals(std::vector<dag::ClusterInfo>{
        {.id = {first_child_node, 0}, .error = 2.0, .lod_bounds = first_child_group_bounds, .tight_bounds = first_child_bounds},
        {.id = {second_child_node, 0}, .error = 0.0, .lod_bounds = second_child_bounds, .tight_bounds = second_child_bounds},
    }));
}

TEST_CASE("a group's parent clusters are the clusters of its node that point at it", "[dag][traverse_dag]") {
    const octree::Id node = octree::Id::root();

    const radix::geometry::Aabb3d first_bounds{glm::dvec3{0.0}, glm::dvec3{1.0}};
    const radix::geometry::Aabb3d second_bounds{glm::dvec3{2.0}, glm::dvec3{3.0}};
    const radix::geometry::Aabb3d third_bounds{glm::dvec3{4.0}, glm::dvec3{5.0}};
    const radix::geometry::Aabb3d first_group_bounds{glm::dvec3{6.0}, glm::dvec3{7.0}};
    const radix::geometry::Aabb3d second_group_bounds{glm::dvec3{8.0}, glm::dvec3{9.0}};

    // The clusters of the two groups alternate, so neither group's are a prefix of the node's.
    const TempDirectory directory("traverse_dag");
    write_dag(directory.path(), {
        {node, dag::NodeMetadata{
            .clusters = {
                {.group_index = 1, .bounds = first_bounds},
                {.group_index = 0, .bounds = second_bounds},
                {.group_index = 1, .bounds = third_bounds},
            },
            .groups = {
                {.children = {}, .error = 1.0, .bounds = first_group_bounds},
                {.children = {}, .error = 2.0, .bounds = second_group_bounds},
            },
        }},
    });

    const std::vector<dag::GroupInfo> groups = collect_groups(directory.path(), AMPLE_RESIDENT_BYTES);
    const auto first_group_it = std::ranges::find(groups, dag::GroupId{node, 0}, &dag::GroupInfo::id);
    const auto second_group_it = std::ranges::find(groups, dag::GroupId{node, 1}, &dag::GroupInfo::id);
    REQUIRE(first_group_it != groups.end());
    REQUIRE(second_group_it != groups.end());

    CHECK_THAT(first_group_it->parent_clusters, Catch::Matchers::UnorderedEquals(std::vector<dag::ClusterInfo>{
        {.id = {node, 1}, .error = 1.0, .lod_bounds = first_group_bounds, .tight_bounds = second_bounds},
    }));
    CHECK_THAT(second_group_it->parent_clusters,Catch::Matchers::UnorderedEquals(std::vector<dag::ClusterInfo>{
        {.id = {node, 0}, .error = 2.0, .lod_bounds = second_group_bounds, .tight_bounds = first_bounds},
        {.id = {node, 2}, .error = 2.0, .lod_bounds = second_group_bounds, .tight_bounds = third_bounds},
    }));
}

TEST_CASE("traverse_groups stops when the visitor returns false", "[dag][traverse_dag]") {
    const octree::Id root = octree::Id::root();
    const octree::Id child = root.children().value()[0];
    const radix::geometry::Aabb3d bounds{glm::dvec3{0.0}, glm::dvec3{1.0}};

    const TempDirectory directory("traverse_dag");
    write_dag(directory.path(), {
        {root, dag::NodeMetadata{.clusters = {{.group_index = 0, .bounds = bounds}}, .groups = {{.children = {}, .error = 0.0, .bounds = bounds}}}},
        {child, dag::NodeMetadata{.clusters = {{.group_index = 0, .bounds = bounds}}, .groups = {{.children = {}, .error = 0.0, .bounds = bounds}}}},
    });
    const auto dag = REQUIRE_VALUE(octree::open_folder_indexed<dag::NodeMetadata>(directory.path()));

    uint32_t visit_count = 0;
    const bool walked = REQUIRE_VALUE(dag::traverse_groups(dag, AMPLE_RESIDENT_BYTES, [&](const dag::GroupView &) {
        visit_count++;
        return false;
    }));

    CHECK_FALSE(walked);
    CHECK(visit_count == 1);
}
