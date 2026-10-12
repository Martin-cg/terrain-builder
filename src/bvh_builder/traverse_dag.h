#pragma once

#include <cmath>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <type_traits>
#include <functional>
#include <unordered_map>
#include <vector>

#include <libassert/assert.hpp>

#include "io/Error.h"
#include "octree/Id.h"
#include "octree/IndexMap.h"
#include "octree/OddLevelShifted.h"
#include "range_utils.h"

#include "build.h"
#include "metadata.h"
#include "parent_child_nodes.h"
#include "../dag_builder/storage.h"
#include "error/Result.h"
#include "error/try.h"
#include "storage/BoundedCache.h"
#include "visitor.h"

namespace dag {

struct FlatNode {
    Id id;
    const GroupMetadata &group;
    double parent_error;
};

namespace detail {
// A cluster carries the group it belongs to in the node storing it.
inline FlatNode make_flat_node(const Id &id, const NodeMetadata &metadata, const double parent_error) {
    const uint32_t group_index = metadata.clusters[id.cluster_index].group_index;
    return FlatNode{
        .id = id,
        .group = metadata.groups[group_index],
        .parent_error = parent_error,
    };
}
} // namespace detail

// The parent error of one cluster.
struct ParentError {
    uint32_t cluster_index;
    double error;

    auto operator<=>(const ParentError &) const = default;
};

// The parent errors a node's groups give their child clusters, by the node storing each cluster.
using ParentErrors = std::unordered_map<octree::Id, std::vector<ParentError>>;

// A cache for the parent errors each dag node's groups give their child clusters, bounded by their size in bytes.
class ParentErrorCache {
public:
    ParentErrorCache(const octree::IndexedDagMetaStorage &dag, const size_t max_resident_bytes)
        : _dag(dag), _cache(max_resident_bytes) {}

    // The parent errors `node`'s groups give their child clusters, read if not resident, valid until the next read.
    [[nodiscard]]
    alp::Result<std::reference_wrapper<const ParentErrors>, io::Error> read_parent_errors(const octree::Id &node) {
        return this->_cache.get_or_try_insert_with(node, [&]() -> alp::Result<Cache::Entry, io::Error> {
            ALP_TRY(const NodeMetadata &metadata, this->_dag.load(node));
            ParentErrors parent_errors = collect_parent_errors(metadata);
            const size_t cost = estimate_bytes(parent_errors);
            return Cache::Entry{.value = std::move(parent_errors), .cost = cost};
        });
    }

    // How many reads had to load a node.
    uint32_t count_misses() const {
        return this->_cache.count_misses();
    }

private:
    using Cache = BoundedCache<octree::Id, ParentErrors>;

    // Gathers the parent errors every group of `metadata` gives its child clusters.
    static ParentErrors collect_parent_errors(const NodeMetadata &metadata) {
        ParentErrors parent_errors;
        for (const GroupMetadata &group : metadata.groups) {
            DEBUG_ASSERT(std::isfinite(group.error));
            for (const Id &child : group.children) {
                parent_errors[child.source_batch].push_back(ParentError{.cluster_index = child.cluster_index, .error = group.error});
            }
        }
        return parent_errors;
    }

    // Roughly how many bytes `parent_errors` holds.
    static size_t estimate_bytes(const ParentErrors &parent_errors) {
        size_t bytes = sizeof(ParentErrors) + parent_errors.bucket_count() * sizeof(void *);
        for (const auto &[node, errors] : parent_errors) {
            bytes += sizeof(ParentErrors::value_type) + errors.capacity() * sizeof(ParentError);
        }
        return bytes;
    }

    const octree::IndexedDagMetaStorage &_dag;
    Cache _cache;
};

// Finds the parent errors of a dag node's clusters among the nodes whose groups may list them.
class ParentErrorLookup {
public:
    ParentErrorLookup(const octree::IndexedDagMetaStorage &dag, const octree::OddLevelShifted &shifted_space, const size_t max_resident_bytes)
        : _dag(dag), _space(shifted_space), _cache(dag, max_resident_bytes) {}

    // The parent error of each of `node`'s `cluster_count` clusters, infinity for those no group lists.
    [[nodiscard]]
    alp::Result<std::vector<double>, io::Error> find_parent_errors(const octree::Id &node, const uint32_t cluster_count) {
        const double infinity = std::numeric_limits<double>::infinity();
        std::vector<double> parent_error_by_cluster(cluster_count, infinity);

        for (const octree::Id &parent_id : find_possible_parent_nodes(node, this->_space)) {
            if (!this->_dag.index().has_data(parent_id)) {
                continue;
            }

            ALP_TRY(const ParentErrors &parent_errors, this->_cache.read_parent_errors(parent_id));
            const auto entry = parent_errors.find(node);
            if (entry == parent_errors.end()) {
                continue;
            }

            // No group error is infinite, so a slot holding anything else was already given one by another group.
            for (const ParentError &parent_error : entry->second) {
                ASSERT(parent_error.cluster_index < cluster_count, "a group lists a cluster its node does not hold");
                ASSERT(parent_error_by_cluster[parent_error.cluster_index] == infinity, "a cluster is listed by two groups");
                parent_error_by_cluster[parent_error.cluster_index] = parent_error.error;
            }
        }
        return parent_error_by_cluster;
    }

private:
    const octree::IndexedDagMetaStorage &_dag;
    const octree::OddLevelShifted &_space;
    ParentErrorCache _cache;
};

// Visits every cluster in the dag with its own group and the error of the group listing it, or infinity if none does. False if the visitor stopped the walk before its end.
template <typename F>
[[nodiscard]]
alp::Result<bool, io::Error> traverse_nodes(
    const octree::IndexedDagMetaStorage &dag, const octree::OddLevelShifted &shifted_space,
    const size_t max_resident_bytes, F &&v) {
    visitor::FallibleVisitor<io::Error, F &> visitor(v);
    ParentErrorLookup lookup(dag, shifted_space, max_resident_bytes);

    // Every child a group lists is a non-root cluster, so both counts meet at the end.
    uint64_t child_count = 0;
    uint64_t non_root_count = 0;

    const octree::IndexMap &index = dag.index();
    for (const auto &[node_id, status] : index) {
        // Skip virtual nodes
        if (!index.has_data(node_id)) {
            continue;
        }

        ALP_TRY(const auto &node, dag.load(node_id));
        const auto cluster_count = static_cast<uint32_t>(node.clusters.size());
        ALP_TRY(const std::vector<double> parent_error_by_cluster, lookup.find_parent_errors(node_id, cluster_count));

        for (const GroupMetadata &group : node.groups) {
            child_count += group.children.size();
        }
        for (const double parent_error : parent_error_by_cluster) {
            if (parent_error != std::numeric_limits<double>::infinity()) {
                non_root_count++;
            }
        }

        for (const uint32_t cluster_index : range(cluster_count)) {
            const Id id{
                .source_batch = node_id,
                .cluster_index = cluster_index};
            const FlatNode flat_node = detail::make_flat_node(id, node, parent_error_by_cluster[cluster_index]);
            ALP_TRY(const bool keep_going, visitor(flat_node));
            if (!keep_going) {
                return false;
            }
        }
    }

    ASSERT(child_count == non_root_count, "a group lists a child that no parent search reached");

    // We walked to the end
    return true;
}

}