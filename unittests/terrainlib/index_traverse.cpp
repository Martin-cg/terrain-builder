#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <utility>
#include <vector>

#include "octree/Id.h"
#include "octree/IndexMap.h"
#include "octree/traverse.h"

TEST_CASE("octree::traverse basic", "[octree][traverse]") {
    octree::IndexMap index;

    const octree::Id root = octree::Id::root();
    index.add(root);

    const auto children = root.children().value();
    for (const auto& child : children) {
        index.add(child);
    }

    SECTION("traversal visits root then children") {
        std::vector<octree::Id> visited;
        octree::traverse(
            index,
            [&](const octree::Id &id, const octree::NodeStatus &) {
                visited.push_back(id);
            },
            [](const octree::Id &) { return true; }, // refine always
            root,
            octree::TraversalOrder::DepthFirst);

        CHECK(visited.front() == root);
        CHECK(visited.size() == 9); // root + 8 children
        std::vector<octree::Id> expected(children.begin(), children.end());
        visited.erase(visited.begin());
        CHECK(visited == expected);
    }

    SECTION("Refine function disables recursion") {
        std::vector<octree::Id> visited;
        octree::traverse(
            index,
            [&](const octree::Id &id, const octree::NodeStatus &) {
                visited.push_back(id);
            },
            [](const octree::Id &) { return false; }, // don't refine
            root,
            octree::TraversalOrder::DepthFirst);

        CHECK(visited.size() == 1);
        CHECK(visited.front() == root);
    }
}

TEST_CASE("octree::traverse skips when the start node is missing", "[octree][traverse]") {
    octree::IndexMap index;
    index.add(octree::Id::root());
    const auto order = GENERATE(octree::TraversalOrder::DepthFirst, octree::TraversalOrder::BreadthFirst);
    bool visited = false;

    octree::traverse(
        index,
        [&](const octree::Id&, const octree::NodeStatus&) {
            visited = true;
        },
        [](const octree::Id&) { return true; },
        octree::Id(1, 5),
        order
    );

    CHECK_FALSE(visited);
}

TEST_CASE("octree::traverse with depth", "[octree][traverse]") {
    octree::IndexMap index;

    const octree::Id root = octree::Id::root();
    index.add(root);

    const octree::Id c0(1, 0);
    const octree::Id c1(1, 1);
    const octree::Id c2(1, 2);

    index.add(c0);
    index.add(c1);
    index.add(c2);

    // Add grandchildren
    const octree::Id gc0 = c0.child(0).value(); // grandchild of c0
    const octree::Id gc1 = c2.child(1).value(); // grandchild of c2

    index.add(gc0);
    index.add(gc1);

    index.remove(c0);

    SECTION("DFS visits deeply before siblings") {
        std::vector<std::pair<octree::Id, octree::NodeStatus>> visited;
        octree::traverse(
            index,
            [&](const octree::Id& id, const octree::NodeStatus& status) {
                visited.emplace_back(id, status);
            },
            [](const octree::Id&) { return true; },
            root,
            octree::TraversalOrder::DepthFirst
        );

        const std::vector<std::pair<octree::Id, octree::NodeStatus>> expected{
            {root, octree::NodeStatus::Inner},
            {c0, octree::NodeStatus::Virtual},
            {gc0, octree::NodeStatus::Leaf},
            {c1, octree::NodeStatus::Leaf},
            {c2, octree::NodeStatus::Inner},
            {gc1, octree::NodeStatus::Leaf}};
        CHECK(visited == expected);
    }

    SECTION("BFS visits all nodes level by level") {
        std::vector<std::pair<octree::Id, octree::NodeStatus>> visited;
        octree::traverse(
            index,
            [&](const octree::Id &id, const octree::NodeStatus &status) {
                visited.emplace_back(id, status);
            },
            [](const octree::Id &) { return true; },
            root,
            octree::TraversalOrder::BreadthFirst);

        const std::vector<std::pair<octree::Id, octree::NodeStatus>> expected{
            {root, octree::NodeStatus::Inner},
            {c0, octree::NodeStatus::Virtual},
            {c1, octree::NodeStatus::Leaf},
            {c2, octree::NodeStatus::Inner},
            {gc0, octree::NodeStatus::Leaf},
            {gc1, octree::NodeStatus::Leaf}};
        CHECK(visited == expected);
    }

    SECTION("Traversal starts at the given node") {
        std::vector<octree::Id> visited;
        octree::traverse(
            index,
            [&](const octree::Id &id, const octree::NodeStatus &) {
                visited.push_back(id);
            },
            [](const octree::Id &) { return true; },
            c2,
            octree::TraversalOrder::DepthFirst);

        const std::vector<octree::Id> expected{c2, gc1};
        CHECK(visited == expected);
    }

    SECTION("Refine function stops recursion only below rejected nodes") {
        std::vector<octree::Id> visited;
        octree::traverse(
            index,
            [&](const octree::Id &id, const octree::NodeStatus &) {
                visited.push_back(id);
            },
            [&](const octree::Id &id) { return id != c0; },
            root,
            octree::TraversalOrder::DepthFirst);

        const std::vector<octree::Id> expected{root, c0, c1, c2, gc1};
        CHECK(visited == expected);
    }
}
