#include "../catch2_helpers.h"
#include "../result_helpers.h"
#include "../temp_directory.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>

#include "../dag_builder/dag_node.h"
#include "../dag_builder/encoded.h"
#include "../dag_builder/storage.h"
#include "octree/OddLevelShifted.h"
#include "octree/storage/open.h"
#include "traverse_dag.h"

namespace {

constexpr double INFINITE_ERROR = std::numeric_limits<double>::infinity();
constexpr size_t AMPLE_RESIDENT_BYTES = 1024 * 1024;

// Metadata giving each cluster a group of its own with the matching one of `errors`.
dag::NodeMetadata make_metadata(const std::vector<double> &errors) {
    dag::NodeMetadata metadata;
    for (uint32_t i = 0; i < errors.size(); i++) {
        metadata.group_assignment.push_back(i);
        metadata.groups.push_back(dag::Group{.children = {}, .error = errors[i], .bounds = {}});
    }
    return metadata;
}

// What the walk handed over, kept past the visit.
struct Visited {
    dag::Group group;
    double parent_error;

    bool operator==(const Visited &) const = default;
};

// A dag folder holding exactly the given nodes.
void write_dag(const std::filesystem::path &path, std::vector<std::pair<octree::Id, dag::NodeMetadata>> nodes) {
    auto dag = REQUIRE_VALUE(octree::open_folder_indexed<dag::ClusterBatch>(path));
    for (auto &[id, metadata] : nodes) {
        REQUIRE(dag.save(id, dag::ClusterBatch{std::move(metadata), Clustering{}}).has_value());
    }
    REQUIRE(dag.save_index().has_value());
}

std::unordered_map<dag::Id, Visited> traverse(const std::filesystem::path &dag_path, const size_t max_resident_bytes = AMPLE_RESIDENT_BYTES) {
    const auto dag = REQUIRE_VALUE(octree::open_folder_indexed<dag::NodeMetadata>(dag_path));
    const octree::OddLevelShifted shifted_space = octree::OddLevelShifted::earth();

    std::unordered_map<dag::Id, Visited> visited;
    const auto collect = [&](const dag::FlatNode &node) {
        const auto [entry, inserted] = visited.emplace(node.id, Visited{.group = node.group, .parent_error = node.parent_error});
        REQUIRE(inserted);
    };
    REQUIRE(dag::traverse_nodes(dag, shifted_space, max_resident_bytes, collect).value_or(false));

    return visited;
}

}

TEST_CASE("every cluster is visited with the group it is assigned to", "[traverse_dag]") {
    const octree::Id root = octree::Id::root();
    const std::vector<dag::Group> groups{
        dag::Group{.children = {}, .error = 1.0, .bounds = {glm::dvec3{0.0}, glm::dvec3{1.0}}},
        dag::Group{.children = {}, .error = 2.0, .bounds = {glm::dvec3{2.0}, glm::dvec3{3.0}}},
    };
    dag::NodeMetadata metadata;
    metadata.group_assignment = {1, 0, 1};
    metadata.groups = groups;

    const TempDirectory directory("traverse_dag");
    write_dag(directory.path(), {{root, std::move(metadata)}});

    const auto visited = traverse(directory.path());

    REQUIRE(visited.size() == 3);
    CHECK(visited.at(dag::Id{root, 0}).group == groups[1]);
    CHECK(visited.at(dag::Id{root, 1}).group == groups[0]);
    CHECK(visited.at(dag::Id{root, 2}).group == groups[1]);
}

TEST_CASE("a cluster takes the error of the group that owns it", "[traverse_dag]") {
    const octree::Id root = octree::Id::root();
    const octree::Id child = root.children().value()[0];

    dag::NodeMetadata coarse = make_metadata({5.0});
    coarse.groups[0].children = {dag::Id{child, 0}, dag::Id{child, 1}};

    const TempDirectory directory("traverse_dag");
    write_dag(directory.path(), {{root, std::move(coarse)}, {child, make_metadata({1.0, 2.0})}});

    const auto visited = traverse(directory.path());

    REQUIRE(visited.size() == 3);
    CHECK(visited.at(dag::Id{root, 0}).parent_error == INFINITE_ERROR);
    CHECK(visited.at(dag::Id{child, 0}).parent_error == 5.0);
    CHECK(visited.at(dag::Id{child, 1}).parent_error == 5.0);
    CHECK(visited.at(dag::Id{child, 0}).group.error == 1.0);
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

    dag::NodeMetadata owner = make_metadata({7.0});
    owner.groups[0].children = {dag::Id{y, 0}};

    const TempDirectory directory("traverse_dag");
    write_dag(directory.path(), {
        {a, std::move(owner)},
        {z, make_metadata({1.0})},
        {y, make_metadata({2.0})},
    });

    const auto visited = traverse(directory.path());

    REQUIRE(visited.size() == 3);
    CHECK(visited.at(dag::Id{y, 0}).parent_error == 7.0);
    CHECK(visited.at(dag::Id{z, 0}).parent_error == INFINITE_ERROR);
    CHECK(visited.at(dag::Id{a, 0}).parent_error == INFINITE_ERROR);
}

TEST_CASE("clusters owned by different groups keep each group's error", "[traverse_dag]") {
    const octree::Id root = octree::Id::root();
    const octree::Id first = root.children().value()[0];
    const octree::Id second = root.children().value()[1];

    dag::NodeMetadata coarse = make_metadata({5.0, 8.0});
    coarse.groups[0].children = {dag::Id{first, 0}};
    coarse.groups[1].children = {dag::Id{second, 0}};

    const TempDirectory directory("traverse_dag");
    write_dag(directory.path(), {
        {root, std::move(coarse)},
        {first, make_metadata({1.0})},
        {second, make_metadata({2.0})},
    });

    const auto visited = traverse(directory.path());

    REQUIRE(visited.size() == 4);
    CHECK(visited.at(dag::Id{first, 0}).parent_error == 5.0);
    CHECK(visited.at(dag::Id{second, 0}).parent_error == 8.0);
}

TEST_CASE("a cluster no group lists is a root, even below the coarsest level", "[traverse_dag]") {
    const octree::Id root = octree::Id::root();
    const octree::Id unlisted = root.children().value()[3].children().value()[5];

    const TempDirectory directory("traverse_dag");
    write_dag(directory.path(), {
        {root, make_metadata({9.0})},
        {unlisted, make_metadata({1.0, 2.0})},
    });

    const auto visited = traverse(directory.path());

    REQUIRE(visited.size() == 3);
    CHECK(visited.at(dag::Id{unlisted, 0}).parent_error == INFINITE_ERROR);
    CHECK(visited.at(dag::Id{unlisted, 1}).parent_error == INFINITE_ERROR);
}

TEST_CASE("each cluster takes the error of its own parent group across three levels", "[traverse_dag]") {
    const octree::Id root = octree::Id::root();
    const octree::Id child = root.children().value()[0];
    const octree::Id grandchild = child.children().value()[0];

    dag::NodeMetadata coarse = make_metadata({9.0});
    coarse.groups[0].children = {dag::Id{child, 0}};
    dag::NodeMetadata middle = make_metadata({4.0});
    middle.groups[0].children = {dag::Id{grandchild, 0}};

    const TempDirectory directory("traverse_dag");
    write_dag(directory.path(), {
        {root, std::move(coarse)},
        {child, std::move(middle)},
        {grandchild, make_metadata({1.0})},
    });

    const auto visited = traverse(directory.path());

    REQUIRE(visited.size() == 3);
    CHECK(visited.at(dag::Id{root, 0}).parent_error == INFINITE_ERROR);
    CHECK(visited.at(dag::Id{child, 0}).parent_error == 9.0);
    CHECK(visited.at(dag::Id{grandchild, 0}).parent_error == 4.0);
}

TEST_CASE("a budget too small to keep any parent gives the same errors", "[traverse_dag]") {
    const octree::Id root = octree::Id::root();
    const octree::Id first = root.children().value()[0];
    const octree::Id second = root.children().value()[1];
    const octree::Id grandchild = first.children().value()[7];

    dag::NodeMetadata coarse = make_metadata({5.0, 8.0});
    coarse.groups[0].children = {dag::Id{first, 0}, dag::Id{second, 1}};
    coarse.groups[1].children = {dag::Id{second, 0}, dag::Id{first, 1}};
    dag::NodeMetadata middle = make_metadata({3.0, 4.0});
    middle.groups[1].children = {dag::Id{grandchild, 0}};

    const TempDirectory directory("traverse_dag");
    write_dag(directory.path(), {
        {root, std::move(coarse)},
        {first, std::move(middle)},
        {second, make_metadata({1.0, 2.0})},
        {grandchild, make_metadata({0.5})},
    });

    const auto unbounded = traverse(directory.path());
    const auto starved = traverse(directory.path(), 1);

    REQUIRE(unbounded.size() == 7);
    CHECK(starved == unbounded);
}

TEST_CASE("a visitor returning false stops the walk", "[traverse_dag]") {
    const octree::Id root = octree::Id::root();
    const octree::Id child = root.children().value()[0];

    dag::NodeMetadata coarse = make_metadata({1.0, 2.0});
    coarse.groups[0].children = {dag::Id{child, 0}};

    const TempDirectory directory("traverse_dag");
    write_dag(directory.path(), {
        {root, std::move(coarse)},
        {child, make_metadata({3.0})},
    });

    const auto dag = REQUIRE_VALUE(octree::open_folder_indexed<dag::NodeMetadata>(directory.path()));

    uint32_t visits = 0;
    const auto stop_after_first = [&](const dag::FlatNode &) {
        visits++;
        return false;
    };
    const auto walked = dag::traverse_nodes(dag, octree::OddLevelShifted::earth(), AMPLE_RESIDENT_BYTES, stop_after_first);
    REQUIRE(walked.has_value());

    CHECK(!walked.value());
    CHECK(visits == 1);
}