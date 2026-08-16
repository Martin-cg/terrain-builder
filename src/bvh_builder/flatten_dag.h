#pragma once

#include <algorithm>
#include <cstddef>
#include <optional>

#include <tl/expected.hpp>

#include "io/Error.h"

#include "../dag_builder/storage.h"

#include "bvh.h"
#include "bvh_utils.h"
#include "storage.h"
#include "traverse_dag.h"

namespace bvh {

namespace detail {

inline size_t bytes_to_batch_size(const size_t bytes) {
    constexpr size_t bytes_per_leaf = sizeof(Node) + sizeof(dag::Id);
    return std::max<size_t>(1, bytes / bytes_per_leaf);
}

}

// Collects dag nodes into batches of bvh leaves.
class LeafWriter {
public:
    LeafWriter(Storage &output, const size_t max_batch_bytes = default_max_batch_bytes)
        : _output(output), _max_leaves(detail::bytes_to_batch_size(max_batch_bytes)) {}

    // Appends `id` as a leaf, writing the batch out once it is full.
    tl::expected<void, io::Error> write(const dag::Id &id, const NodeHeader &header) {
        if (!this->_current) {
            this->_current = this->_output.create_batch();
        }

        emit_leaf_node(*this->_current, header, id);
        if (this->_current->nodes.size() < this->_max_leaves) {
            return {};
        }
        return this->flush();
    }

    // Writes the batch still in progress.
    tl::expected<void, io::Error> flush() {
        if (!this->_current) {
            return {};
        }

        const auto written = this->_output.write(*this->_current);
        this->_current.reset();
        return written;
    }

private:
    Storage &_output;
    size_t _max_leaves;
    std::optional<Batch> _current;
};

struct FlattenOptions {
    size_t max_batch_bytes = default_max_batch_bytes;
};

// The header a dag node carries as a bvh leaf.
inline NodeHeader make_header(const dag::FlatNode &node) {
    return NodeHeader{
        .min_child_error = node.group.error,
        .max_parent_error = node.parent_error,
        .triangle_count = node.triangle_count,
        .bounds = node.group.bounds,
    };
}

// Writes every dag node as a set of bvh leaf batches.
inline tl::expected<void, io::Error> flatten_dag(
    const octree::IndexedDagMetaStorage &dag, Storage &output, const FlattenOptions &options = {}) {
    LeafWriter writer(output, options.max_batch_bytes);

    // The walk stops on the first failed write, leaving it to report.
    tl::expected<void, io::Error> write_result;
    const auto traversal_result = dag::traverse_nodes(dag, [&](const dag::FlatNode &node) {
        write_result = writer.write(node.id, make_header(node));
        return write_result.has_value();
    });
    if (!traversal_result) {
        return tl::unexpected(traversal_result.error());
    }
    if (!traversal_result.value()) {
        return write_result;
    }
    return writer.flush();
}

}