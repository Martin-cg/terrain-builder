#pragma once

#include <cstdint>
#include <expected>
#include <span>
#include <string_view>
#include <utility>

#include <libassert/assert.hpp>

#include "WorkerGroup.h"
#include "containers/ChunkedRingBuffer.h"
#include "containers/ChunkedRingWriter.h"
#include "error/Result.h"
#include "error/try.h"
#include "error/utils.h"
#include "io/Error.h"

#include "bvh/BatchWriter.h"
#include "bvh/Storage.h"
#include "frontier/BatchWriter.h"
#include "frontier/Storage.h"
#include "frontier/layout.h"
#include "frontier/traverse.h"
#include "gated_ploc/AdmissionGate.h"
#include "gated_ploc/ChunkMerger.h"
#include "gated_ploc/options.h"

namespace gated_ploc {

namespace detail {

using EntryRing = ChunkedRingBuffer<frontier::Entry>;
using EntryWriter = ChunkedRingWriter<frontier::Entry>;

// The round ended under this worker before it finished.
struct Cancelled {
    bool operator==(const Cancelled &) const = default;

    friend std::string_view format_as(const Cancelled) {
        return "cancelled";
    }
};

// What a worker hands back: its count, its own failure, or the round ending around it.
using WorkerResult = alp::Result<uint64_t, io::Error, Cancelled>;

// Feeds the ring the entries of the run this round admits and writes the rest to `merged`.
// Returns how many entries it read.
[[nodiscard]]
inline WorkerResult run_producer(
    const frontier::Storage &input, frontier::Storage &merged, EntryRing &ring, const Options &options) {
    frontier::BatchWriter next_run(merged, options.max_batch_bytes);
    EntryWriter admitted(ring);

    AdmissionGate gate(
        [&admitted](const std::span<const frontier::Entry> entries) -> alp::Result<bool, io::Error> {
            return admitted.write(entries).has_value();
        },
        [&next_run](const std::span<const frontier::Entry> entries) -> alp::Result<void, io::Error> {
            for (const frontier::Entry &entry : entries) {
                ALP_TRY(next_run.write(entry));
            }
            return {};
        },
        options.admission);

    uint64_t entries = 0;
    ALP_TRY(const bool completed, frontier::traverse_batches(input, [&](const frontier::Batch &batch) -> alp::Result<bool, io::Error> {
        entries += batch.entries.size();
        return gate.push(batch.entries);
    }));

    if (!completed) {
        return alp::make_unexpected(Cancelled{});
    }

    ALP_TRY(gate.flush());
    std::move(admitted).finish();

    ALP_TRY(next_run.flush());
    return entries;
}

// Merges the chunks of the ring as they arrive and returns how many pairs it merged.
[[nodiscard]]
inline WorkerResult run_consumer(
    EntryRing &ring, frontier::Storage &merged, bvh::Storage &output, const Options &options) {
    bvh::BatchWriter sink(output, options.max_batch_bytes);
    frontier::BatchWriter next_run(merged, options.max_batch_bytes);
    ChunkMerger merger(sink, next_run, options);

    uint64_t merges = 0;
    while (true) {
        const std::expected<EntryRing::ReadLease, EntryRing::StreamEnd> lease_result = ring.rent_for_reading();
        if (!lease_result.has_value()) {
            if (lease_result.error() == EntryRing::StreamEnd::Aborted) {
                return alp::make_unexpected(Cancelled{});
            }
            break;
        }

        alp::Result<uint64_t, io::Error> count_result = merger.merge_chunk(lease_result->view());
        if (!count_result.has_value()) {
            ring.abort();
            return alp::propagate(std::move(count_result).error());
        }
        merges += count_result.value();
    }

    ALP_TRY(next_run.flush());
    ALP_TRY(sink.flush());
    return merges;
}

// Merges what the round admits, and returns how many entries the frontier is left with.
[[nodiscard]]
inline alp::Result<uint64_t, io::Error> run_round(
    const frontier::Storage &input, frontier::Storage &merged, bvh::Storage &output,
    EntryRing &ring, const Options &options) {
    ring.reset();

    WorkerGroup workers(options.worker_count, [&]() {
        return run_consumer(ring, merged, output, options);
    });

    const WorkerResult read_result = run_producer(input, merged, ring, options);

    uint64_t merges = 0;
    for (const WorkerResult &result : workers.join()) {
        if (!result.has_value()) {
            auto narrow_result = result.error().narrow<io::Error>();
            if (narrow_result.has_value()) {
                return alp::propagate(std::move(narrow_result).value());
            }
            continue;
        }
        merges += result.value();
    }

    if (!read_result.has_value()) {
        io::Error io_error = *ASSERT_VAL(
            read_result.error().narrow<io::Error>(),
            "the round was cancelled without any worker failing");
        return alp::propagate(std::move(io_error));
    }

    ASSERT(ring.stream_state() != EntryRing::StreamState::Aborted, "the round aborted without any worker failing");
    return read_result.value() - merges;
}

} // namespace detail

} // namespace gated_ploc
