#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <utility>

#include "error/Result.h"
#include "error/try.h"
#include "io/Error.h"
#include "octree/Id.h"

#include "metadata.h"
#include "../../dag_builder/storage.h"
#include "storage/BoundedCache.h"

namespace dag {

// A cache of dag node metadata, bounded by its size in bytes.
class NodeMetadataCache {
public:
    NodeMetadataCache(const octree::IndexedDagMetaStorage &dag, const size_t max_resident_bytes)
        : _dag(dag), _cache(max_resident_bytes) {}

    // Reads the metadata of `node`.
    // Valid until the next read.
    [[nodiscard]]
    alp::Result<std::reference_wrapper<const NodeMetadata>, io::Error> read_metadata(const octree::Id &node) {
        return this->_cache.get_or_try_insert_with(node, [&]() -> alp::Result<Cache::Entry, io::Error> {
            ALP_TRY(NodeMetadata metadata, this->_dag.load(node));
            const size_t cost = estimate_bytes(metadata);
            return Cache::Entry{.value = std::move(metadata), .cost = cost};
        });
    }

    // How many reads had to load a node.
    uint32_t count_misses() const {
        return this->_cache.count_misses();
    }

private:
    using Cache = BoundedCache<octree::Id, NodeMetadata>;

    // Estimates how many bytes `metadata` holds.
    static size_t estimate_bytes(const NodeMetadata &metadata) {
        size_t bytes = sizeof(NodeMetadata) + metadata.clusters.capacity() * sizeof(ClusterMetadata) + metadata.groups.capacity() * sizeof(GroupMetadata);
        for (const GroupMetadata &group : metadata.groups) {
            bytes += group.children.capacity() * sizeof(Id);
        }
        return bytes;
    }

    const octree::IndexedDagMetaStorage &_dag;
    Cache _cache;
};

} // namespace dag
