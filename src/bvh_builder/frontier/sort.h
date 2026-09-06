#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>
#include <ranges>

#include <libassert/assert.hpp>
#include <tl/expected.hpp>

#include "enumerate.h"
#include "io/Error.h"
#include "range_utils.h"

#include "morton/PrefixMap.h"
#include "frontier/Storage.h"
#include "frontier/traverse.h"
#include "frontier/layout.h"
#include "frontier/Storage.h"
#include "storage/batch_limits.h"
#include "try.h"

namespace frontier {

// Entries in ascending morton order.
inline void sort_entries(const std::span<Entry> entries) {
    std::ranges::sort(entries, {}, &Entry::morton);
}

// Sorts every batch of the run in itself.
inline tl::expected<void, io::Error> sort_batches_independently(Storage &run) {
    for (const uint32_t index : range(run.size())) {
        TRY(Batch batch, run.read(index));
        sort_entries(batch.entries);
        TRY(run.write(index, batch));
    }
    return {};
}

// Entry counts per morton prefix cell.
inline tl::expected<morton::PrefixMap<uint64_t>, io::Error> build_histogram(const Storage &input, const uint32_t prefix_bits) {
    morton::PrefixMap<uint64_t> histogram(prefix_bits, 0);

    TRY(traverse_entries(input, [&](const Entry &entry) {
        histogram.at(entry.morton)++;
    }));

    return histogram;
}

struct Buckets {
    morton::PrefixMap<uint32_t> assignment;
    uint32_t count;
};

// The bucket each cell belongs to, filling buckets to at most `max_entries`.
inline Buckets assign_buckets(const morton::PrefixMap<uint64_t> &histogram, const uint64_t max_entries) {
    DEBUG_ASSERT(max_entries > 0);

    morton::PrefixMap<uint32_t> buckets(histogram.prefix_bits(), 0);
    const std::span<uint32_t> assignment = buckets.cells();

    uint32_t bucket = 0;
    uint64_t filled = 0;
    for (const auto &[cell, count] : enumerate<uint32_t>(histogram.cells())) {
        // Empty cells cost nothing, so they stay with whatever bucket is open.
        const bool has_entries = count > 0;
        const bool would_overflow = filled + count > max_entries;
        // A cell is never split, so one over the limit lands whole in a bucket of its own.
        const bool bucket_is_open = filled > 0;

        if (has_entries && would_overflow && bucket_is_open) {
            bucket++;
            filled = 0;
        }

        assignment[cell] = bucket;
        filled += count;
    }
    return Buckets{.assignment = std::move(buckets), .count = bucket + 1};
}

// Writes one batch per bucket, in morton order.
inline tl::expected<void, io::Error> scatter(const Storage &input, const Buckets &buckets, Storage &output, const uint32_t max_open_batches) {
    DEBUG_ASSERT(max_open_batches > 0);

    // Only so many batches fit in memory, so the input is read once per group of buckets.
    for (uint32_t first = 0; first < buckets.count; first += max_open_batches) {
        const uint32_t end = std::min(buckets.count, first + max_open_batches);

        std::vector<Batch> open;
        for (const uint32_t _ : range(first, end)) {
            open.push_back(Batch{});
        }

        TRY(traverse_entries(input, [&](const Entry &entry) {
            const uint32_t bucket = buckets.assignment.at(entry.morton);
            if (bucket >= first && bucket < end) {
                open[bucket - first].entries.push_back(entry);
            }
        }));

        for (const Batch &batch : open) {
            TRY(output.write(batch));
        }
    }
    return {};
}

struct SortOptions {
    size_t max_batch_bytes = default_max_batch_bytes;
    size_t max_resident_bytes = 4 * default_max_batch_bytes;
    uint32_t prefix_bits = 15;
};

// Writes the run's entries to `output` in ascending morton order.
// A radix sort over the leading code bits: count the cells, cut them into buckets, write one batch per bucket, then order each batch.
inline tl::expected<void, io::Error> sort_by_morton(const Storage &input, Storage &output, const SortOptions &options) {
    TRY(const morton::PrefixMap<uint64_t> &histogram, build_histogram(input, options.prefix_bits));

    // Without entries there is no bucket to write, not even an empty one.
    const bool has_entries = std::ranges::any_of(histogram.cells(), [](const uint64_t count) { return count > 0; });
    if (!has_entries) {
        return {};
    }

    const uint64_t max_entries = std::max<uint64_t>(1, options.max_batch_bytes / sizeof(Entry));
    const auto max_open_batches = static_cast<uint32_t>(std::max<size_t>(1, options.max_resident_bytes / options.max_batch_bytes));
    const Buckets buckets = assign_buckets(histogram, max_entries);

    TRY(scatter(input, buckets, output, max_open_batches));
    return sort_batches_independently(output);
}

}
