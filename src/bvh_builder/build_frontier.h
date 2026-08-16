#pragma once

#include <algorithm>
#include <cstddef>
#include <optional>

#include <tl/expected.hpp>

#include "io/Error.h"

#include "batch_store.h"
#include "bvh.h"
#include "flat_traverse.h"
#include "frontier.h"
#include "morton.h"
#include "storage.h"

namespace frontier {

namespace detail {

inline size_t bytes_to_batch_size(const size_t bytes) {
    return std::max<size_t>(1, bytes / sizeof(Entry));
}

}

// Collects frontier entries into run batches.
class EntryWriter {
public:
    EntryWriter(RunStorage &output, const size_t max_batch_bytes = default_max_batch_bytes)
        : _output(output), _max_entries(detail::bytes_to_batch_size(max_batch_bytes)) {}

    // Appends `entry`, writing the batch out once it is full.
    tl::expected<void, io::Error> write(const Entry &entry) {
        if (!this->_current) {
            this->_current = this->_output.create_batch();
        }

        this->_current->entries.push_back(entry);
        if (this->_current->entries.size() < this->_max_entries) {
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
    RunStorage &_output;
    size_t _max_entries;
    std::optional<Batch> _current;
};

struct BuildOptions {
    size_t max_batch_bytes = default_max_batch_bytes;
};

// The entry a stored node enters the frontier as.
inline Entry make_entry(const bvh::Bounds &scene, const bvh::Reference &ref, const bvh::Node &node) {
    return Entry{
        .morton = morton_of(scene, node.header),
        .header = node.header,
        .ref = ref,
    };
}

// Writes every stored node as a set of frontier run batches.
inline tl::expected<void, io::Error> build_from_leaves(
    const bvh::Storage &nodes, const bvh::Bounds &scene, RunStorage &output, const BuildOptions &options = {}) {
    EntryWriter writer(output, options.max_batch_bytes);

    // The walk stops on the first failed write, leaving it to report.
    tl::expected<void, io::Error> write_result;
    const auto traversal_result = bvh::flat_traverse(nodes, [&](const bvh::Reference &ref, const bvh::Node &node) {
        const auto entry = make_entry(scene, ref, node);
        write_result = writer.write(entry);
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