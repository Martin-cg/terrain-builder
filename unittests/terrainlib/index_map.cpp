#include <cstddef>
#include <optional>
#include <system_error>
#include <unordered_map>
#include <vector>

#include "../catch2_helpers.h"

#include "octree/IndexMap.h"

TEST_CASE("IndexMap::add inserts the root as a leaf", "[octree][IndexMap]") {
    octree::IndexMap index;
    const octree::Id root = octree::Id::root();

    CHECK(index.add(root));

    CHECK(index.size() == 1);
    CHECK(index.is_present(root));
    CHECK(index.is(octree::NodeStatus::Leaf, root));
    CHECK(index.get(root) == octree::NodeStatus::Leaf);
}

TEST_CASE("IndexMap::add returns false for a node that is already present", "[octree][IndexMap]") {
    octree::IndexMap index;
    const octree::Id root = octree::Id::root();

    SECTION("a leaf") {
        index.add(root);

        CHECK_FALSE(index.add(root));
        CHECK(index.is(octree::NodeStatus::Leaf, root));
    }

    SECTION("an inner node") {
        index.add(root);
        index.add(root.child(0).value());

        CHECK_FALSE(index.add(root));
        CHECK(index.is(octree::NodeStatus::Inner, root));
    }
}

TEST_CASE("IndexMap::add creates missing ancestors as virtual nodes", "[octree][IndexMap]") {
    octree::IndexMap index;
    const octree::Id root = octree::Id::root();

    SECTION("one level") {
        const octree::Id child = root.child(3).value();

        CHECK(index.add(child));

        CHECK(index.size() == 2);
        CHECK(index.is(octree::NodeStatus::Virtual, root));
        CHECK(index.is(octree::NodeStatus::Leaf, child));
    }

    SECTION("two levels") {
        const octree::Id child = root.child(3).value();
        const octree::Id grandchild = child.child(1).value();

        CHECK(index.add(grandchild));

        CHECK(index.size() == 3);
        CHECK(index.is(octree::NodeStatus::Virtual, root));
        CHECK(index.is(octree::NodeStatus::Virtual, child));
        CHECK(index.is(octree::NodeStatus::Leaf, grandchild));
    }
}

TEST_CASE("IndexMap::add promotes a leaf parent to inner", "[octree][IndexMap]") {
    octree::IndexMap index;
    const octree::Id root = octree::Id::root();
    const octree::Id child = root.child(0).value();
    index.add(root);

    CHECK(index.add(child));

    CHECK(index.is(octree::NodeStatus::Inner, root));
    CHECK(index.is(octree::NodeStatus::Leaf, child));
}

TEST_CASE("IndexMap::add promotes a virtual node to inner", "[octree][IndexMap]") {
    octree::IndexMap index;
    const octree::Id root = octree::Id::root();
    const octree::Id child = root.child(0).value();
    index.add(child);
    REQUIRE(index.is(octree::NodeStatus::Virtual, root));

    CHECK(index.add(root));

    CHECK(index.is(octree::NodeStatus::Inner, root));
    CHECK(index.is(octree::NodeStatus::Leaf, child));
}

TEST_CASE("IndexMap::remove returns false for an absent node", "[octree][IndexMap]") {
    octree::IndexMap index;

    CHECK_FALSE(index.remove(octree::Id::root()));

    CHECK(index.empty());
}

TEST_CASE("IndexMap::remove erases a leaf together with its virtual ancestors", "[octree][IndexMap]") {
    octree::IndexMap index;
    const octree::Id root = octree::Id::root();
    const octree::Id child = root.child(2).value();
    const octree::Id grandchild = child.child(1).value();
    index.add(grandchild);
    REQUIRE(index.size() == 3);

    CHECK(index.remove(grandchild));

    CHECK(index.is_absent(grandchild));
    CHECK(index.is_absent(child));
    CHECK(index.is_absent(root));
    CHECK(index.empty());
}

TEST_CASE("IndexMap::remove keeps an inner parent while another child remains", "[octree][IndexMap]") {
    octree::IndexMap index;
    const octree::Id root = octree::Id::root();
    const octree::Id first_child = root.child(0).value();
    const octree::Id second_child = root.child(1).value();
    index.add(root);
    index.add(first_child);
    index.add(second_child);

    CHECK(index.remove(first_child));

    CHECK(index.is_absent(first_child));
    CHECK(index.is(octree::NodeStatus::Leaf, second_child));
    CHECK(index.is(octree::NodeStatus::Inner, root));
}

TEST_CASE("IndexMap::remove turns a parent without further children into a leaf", "[octree][IndexMap]") {
    octree::IndexMap index;
    const octree::Id root = octree::Id::root();
    const octree::Id child = root.child(0).value();
    index.add(root);
    index.add(child);

    CHECK(index.remove(child));

    CHECK(index.is_absent(child));
    CHECK(index.is(octree::NodeStatus::Leaf, root));
}

TEST_CASE("IndexMap::remove turns an inner node virtual", "[octree][IndexMap]") {
    octree::IndexMap index;
    const octree::Id root = octree::Id::root();
    const octree::Id child = root.child(0).value();
    index.add(root);
    index.add(child);
    REQUIRE(index.is(octree::NodeStatus::Inner, root));

    CHECK(index.remove(root));

    CHECK(index.is(octree::NodeStatus::Virtual, root));
    CHECK(index.is(octree::NodeStatus::Leaf, child));
}

TEST_CASE("IndexMap::remove returns false for a virtual node", "[octree][IndexMap]") {
    octree::IndexMap index;
    const octree::Id root = octree::Id::root();
    const octree::Id child = root.child(0).value();
    index.add(child);
    REQUIRE(index.is(octree::NodeStatus::Virtual, root));

    CHECK_FALSE(index.remove(root));

    CHECK(index.size() == 2);
    CHECK(index.is(octree::NodeStatus::Virtual, root));
    CHECK(index.is(octree::NodeStatus::Leaf, child));
}

TEST_CASE("IndexMap::get returns nothing for an absent node", "[octree][IndexMap]") {
    octree::IndexMap index;
    const octree::Id root = octree::Id::root();
    const octree::Id child = root.child(0).value();
    index.add(root);

    CHECK(index.get(child) == std::nullopt);
    CHECK(index.is_absent(child));
}

TEST_CASE("IndexMap::clear empties the map", "[octree][IndexMap]") {
    octree::IndexMap index;
    const octree::Id root = octree::Id::root();
    index.add(root.child(1).value());
    index.add(root.child(2).value());
    REQUIRE(index.size() == 3);

    index.clear();

    CHECK(index.empty());
    CHECK(index.size() == 0);
    CHECK(index.is_absent(root));
}

TEST_CASE("IndexMap iterates over every node with its status", "[octree][IndexMap]") {
    octree::IndexMap index;
    const octree::Id root = octree::Id::root();
    const octree::Id child = root.child(2).value();
    const octree::Id grandchild = child.child(1).value();
    index.add(grandchild);
    index.add(child);

    const std::unordered_map<octree::Id, octree::NodeStatus> expected{
        {root, octree::NodeStatus::Virtual},
        {child, octree::NodeStatus::Inner},
        {grandchild, octree::NodeStatus::Leaf},
    };

    const std::unordered_map<octree::Id, octree::NodeStatus> visited(index.begin(), index.end());
    const std::unordered_map<octree::Id, octree::NodeStatus> visited_const(index.cbegin(), index.cend());

    CHECK(visited == expected);
    CHECK(visited_const == expected);
}

TEST_CASE("IndexMap survives a serialization round trip", "[octree][IndexMap]") {
    octree::IndexMap original;
    const octree::Id root = octree::Id::root();
    const octree::Id child = root.child(3).value();
    const octree::Id grandchild = child.child(1).value();
    original.add(grandchild);
    original.add(child);

    const std::errc success{};

    std::vector<std::byte> buffer;
    zpp::bits::out out(buffer);
    REQUIRE(out(original).code == success);

    octree::IndexMap loaded;
    zpp::bits::in in(buffer);
    REQUIRE(in(loaded).code == success);

    CHECK(loaded == original);
}
