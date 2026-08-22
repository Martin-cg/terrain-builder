#include "../catch2_helpers.h"
#include "../temp_directory.h"

#include <limits>
#include <unordered_map>
#include <vector>

#include "../dag_builder/dag_node.h"
#include "../dag_builder/encoded.h"
#include "../dag_builder/storage.h"
#include "octree/storage/open.h"
#include "traverse_dag.h"

namespace {

constexpr double infinity = std::numeric_limits<double>::infinity();

radix::geometry::Aabb3d box_at(const double offset) {
    return radix::geometry::Aabb3d{glm::dvec3{offset}, glm::dvec3{offset + 1.0}};
}

// One group per cluster, so cluster i carries error, bounds and triangle count of group i.
dag::NodeMetadata metadata_with_clusters(const std::vector<double> &errors) {
    dag::NodeMetadata metadata;
    for (uint32_t i = 0; i < errors.size(); i++) {
        metadata.group_assignment.push_back(i);
        metadata.triangle_counts.push_back(100 + i);
        metadata.groups.push_back(dag::Group{
            .children = {},
            .error = errors[i],
            .bounds = box_at(i),
        });
    }
    return metadata;
}

// What the walk handed over, kept past the visit.
struct Visited {
    double error;
    radix::geometry::Aabb3d bounds;
    uint32_t triangle_count;
    double parent_error;
};

// A dag folder holding exactly the given nodes.
void write_dag(const std::filesystem::path &path, std::vector<std::pair<octree::Id, dag::NodeMetadata>> nodes) {
    auto dag = octree::open_folder_indexed<dag::ClusterBatch>(path);
    for (auto &[id, metadata] : nodes) {
        REQUIRE(dag.save(id, dag::ClusterBatch{std::move(metadata), Clustering{}}).has_value());
    }
    REQUIRE(dag.save_index().has_value());
}

std::unordered_map<dag::Id, Visited> traverse(const std::filesystem::path &dag_path) {
    const auto dag = octree::open_folder_indexed<dag::NodeMetadata>(dag_path);

    std::unordered_map<dag::Id, Visited> visited;
    const auto collect = [&](const dag::FlatNode &node) {
        const auto [entry, inserted] = visited.emplace(node.id, Visited{
            .error = node.group.error,
            .bounds = node.group.bounds,
            .triangle_count = node.triangle_count,
            .parent_error = node.parent_error,
        });
        REQUIRE(inserted);
    };
    REQUIRE(dag::traverse_nodes(dag, collect).value_or(false));

    return visited;
}

}

TEST_CASE("every cluster is visited with its group's error, bounds and triangle count", "[traverse_dag]") {
    const TempDirectory directory("traverse_dag");
    write_dag(directory.path(), {{octree::Id::root(), metadata_with_clusters({1.0, 2.0, 3.0})}});

    const auto visited = traverse(directory.path());

    REQUIRE(visited.size() == 3);
    for (uint32_t cluster = 0; cluster < 3; cluster++) {
        const Visited &node = visited.at(dag::Id{octree::Id::root(), cluster});
        CHECK(node.error == cluster + 1.0);
        CHECK(node.parent_error == infinity);
        CHECK(node.triangle_count == 100 + cluster);
        CHECK(node.bounds == box_at(cluster));
    }
}

TEST_CASE("a cluster takes the error of the group that owns it", "[traverse_dag]") {
    const octree::Id root = octree::Id::root();
    const octree::Id child = root.children().value()[0];

    dag::NodeMetadata coarse = metadata_with_clusters({5.0});
    coarse.groups[0].children = {dag::Id{child, 0}, dag::Id{child, 1}};

    const TempDirectory directory("traverse_dag");
    write_dag(directory.path(), {{root, std::move(coarse)}, {child, metadata_with_clusters({1.0, 2.0})}});

    const auto visited = traverse(directory.path());

    REQUIRE(visited.size() == 3);
    CHECK(visited.at(dag::Id{root, 0}).parent_error == infinity);
    CHECK(visited.at(dag::Id{child, 0}).parent_error == 5.0);
    CHECK(visited.at(dag::Id{child, 1}).parent_error == 5.0);
    CHECK(visited.at(dag::Id{child, 0}).error == 1.0);
}

TEST_CASE("a cluster is owned by a node that is not its octree ancestor", "[traverse_dag]") {
    // The owner A holds a cluster stored in Y, which hangs under A's neighbour Z, so no ancestor of
    // Y ever names its clusters.
    const octree::Id a = octree::Id::root().children().value()[0].children().value()[0];
    const octree::Id z = octree::Id::root().children().value()[1].children().value()[0];
    const octree::Id y = z.children().value()[0];
    REQUIRE_FALSE(a.is_ancestor_of(y, true));

    // Z sits at A's own level, so its clusters are roots like A's.
    REQUIRE(z.level() == a.level());

    dag::NodeMetadata owner = metadata_with_clusters({7.0});
    owner.groups[0].children = {dag::Id{y, 0}};

    const TempDirectory directory("traverse_dag");
    write_dag(directory.path(), {
        {a, std::move(owner)},
        {z, metadata_with_clusters({1.0})},
        {y, metadata_with_clusters({2.0})},
    });

    const auto visited = traverse(directory.path());

    REQUIRE(visited.size() == 3);
    CHECK(visited.at(dag::Id{y, 0}).parent_error == 7.0);
    CHECK(visited.at(dag::Id{z, 0}).parent_error == infinity);
    CHECK(visited.at(dag::Id{a, 0}).parent_error == infinity);
}

TEST_CASE("clusters owned by different groups keep each group's error", "[traverse_dag]") {
    const octree::Id root = octree::Id::root();
    const octree::Id first = root.children().value()[0];
    const octree::Id second = root.children().value()[1];

    dag::NodeMetadata coarse = metadata_with_clusters({5.0, 8.0});
    coarse.groups[0].children = {dag::Id{first, 0}};
    coarse.groups[1].children = {dag::Id{second, 0}};

    const TempDirectory directory("traverse_dag");
    write_dag(directory.path(), {
        {root, std::move(coarse)},
        {first, metadata_with_clusters({1.0})},
        {second, metadata_with_clusters({2.0})},
    });

    const auto visited = traverse(directory.path());

    REQUIRE(visited.size() == 4);
    CHECK(visited.at(dag::Id{first, 0}).parent_error == 5.0);
    CHECK(visited.at(dag::Id{second, 0}).parent_error == 8.0);
}

TEST_CASE("a visitor returning false stops the walk", "[traverse_dag]") {
    const octree::Id root = octree::Id::root();
    const octree::Id child = root.children().value()[0];

    dag::NodeMetadata coarse = metadata_with_clusters({1.0, 2.0});
    coarse.groups[0].children = {dag::Id{child, 0}};

    const TempDirectory directory("traverse_dag");
    write_dag(directory.path(), {
        {root, std::move(coarse)},
        {child, metadata_with_clusters({3.0})},
    });

    const auto dag = octree::open_folder_indexed<dag::NodeMetadata>(directory.path());

    uint32_t visits = 0;
    const auto stop_after_first = [&](const dag::FlatNode &) {
        visits++;
        return false;
    };
    const auto walked = dag::traverse_nodes(dag, stop_after_first);
    REQUIRE(walked.has_value());

    CHECK(!walked.value());
    CHECK(visits == 1);
}