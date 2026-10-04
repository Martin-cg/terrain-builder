#include "../catch2_helpers.h"

#include <vector>

#include "frontier/layout.h"
#include "frontier/sort.h"

TEST_CASE("sort_entries orders by morton", "[frontier][sort]") {
    const frontier::Entry a{.morton = 6, .header = {}, .ref = {.batch = 0, .node = 0}};
    const frontier::Entry b{.morton = 1, .header = {}, .ref = {.batch = 0, .node = 1}};
    const frontier::Entry c{.morton = 3, .header = {}, .ref = {.batch = 0, .node = 2}};

    std::vector<frontier::Entry> entries = {a, b, c};
    frontier::sort_entries(entries);

    CHECK(entries == std::vector<frontier::Entry>{b, c, a});
}

TEST_CASE("sort_entries orders equal codes by header, then by reference", "[frontier][sort]") {
    // The header decides although the references say the opposite, and the reference only between equal headers.
    const frontier::Entry a{.morton = 5, .header = {.min_child_error = 2.0, .max_parent_error = 4.0, .bounds = {}}, .ref = {.batch = 0, .node = 0}};
    const frontier::Entry b{.morton = 5, .header = {.min_child_error = 1.0, .max_parent_error = 4.0, .bounds = {}}, .ref = {.batch = 0, .node = 3}};
    const frontier::Entry c{.morton = 5, .header = {.min_child_error = 1.0, .max_parent_error = 4.0, .bounds = {}}, .ref = {.batch = 0, .node = 2}};

    std::vector<frontier::Entry> entries = {a, b, c};
    frontier::sort_entries(entries);

    CHECK(entries == std::vector<frontier::Entry>{c, b, a});
}
