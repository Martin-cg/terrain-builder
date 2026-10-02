#include <cstdint>
#include <vector>

#include "../catch2_helpers.h"

#include "Partitioning.h"

TEST_CASE("remove_empty_partitions_inplace renumbers the used partitions in order", "[Partitioning]") {
    // Partitions 1 and 2 are empty, and the used ones appear in reverse order.
    Partitioning partitioning{.partition_count = 4, .clusters_per_partition = 2, .cluster_partitions = {3, 0, 3, 0}};

    remove_empty_partitions_inplace(partitioning);

    CHECK(partitioning.partition_count == 2);
    CHECK(partitioning.clusters_per_partition == 2);
    CHECK(partitioning.cluster_partitions == std::vector<uint32_t>{1, 0, 1, 0});
}
