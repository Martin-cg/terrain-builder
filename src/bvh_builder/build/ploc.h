#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <iterator>
#include <limits>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include <fmt/format.h>
#include <libassert/assert.hpp>

#include "containers/ChunkedRingBuffer.h"
#include "containers/ChunkedRingWriter.h"
#include "geometry/geometry.h"
#include "io/Error.h"
#include "log.h"
#include "numeric/int_math.h"
#include "range_utils.h"
#include "error/Result.h"
#include "error/try.h"
#include "error/utils.h"
#include "WorkerGroup.h"

#include "storage/batch_limits.h"
#include "build/merge_cost.h"
#include "bvh/BatchWriter.h"
#include "bvh/Storage.h"
#include "bvh/earth.h"
#include "frontier/BatchWriter.h"
#include "frontier/Storage.h"
#include "frontier/layout.h"
#include "frontier/merge_entry.h"
#include "frontier/sort.h"
#include "frontier/traverse.h"

namespace ploc {

struct AdmissionOptions {
    // Entries one limit is taken over.
    size_t region_size = 1 << 16;
    // The order statistic a region measures its small nodes by.
    double quantile = 0.33;
    // How much larger than that a node may be and still merge.
    double tolerance = 3.0;
};

struct Options {
    size_t search_radius = 16;
    double error_weight = 1.0;
    bvh::Bounds scene_bounds = bvh::earth_bounds();
    AdmissionOptions admission = {};

    size_t chunk_size = 1 << 15;
    size_t chunk_count = 32;
    size_t worker_count = 8;
    size_t max_batch_bytes = default_max_batch_bytes;
    size_t max_resident_bytes = 4 * default_max_batch_bytes;
    uint32_t sort_prefix_bits = 15;
};

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

// The extent up to which a region admits: enough for its two smallest nodes to find each other, and as much more around them as the tolerance allows.
[[nodiscard]]
inline double compute_admission_limit(std::vector<double> &metrics, const AdmissionOptions &options) {
    DEBUG_ASSERT(!metrics.empty());

    const auto count = static_cast<std::ptrdiff_t>(metrics.size());
    const std::ptrdiff_t second = std::min<std::ptrdiff_t>(1, count - 1);
    std::ranges::nth_element(metrics, metrics.begin() + second);
    const double smallest_pair = metrics[second];

    const auto index = static_cast<std::ptrdiff_t>(options.quantile * (count - 1));
    std::ranges::nth_element(metrics, metrics.begin() + index);
    return std::max(smallest_pair, metrics[index] * options.tolerance);
}

// The cheapest partner found for an entry, and what it costs.
struct Nearest {
    std::optional<size_t> index;
    double cost = std::numeric_limits<double>::infinity();

    // Takes `partner` if it is cheaper than the one held, reporting whether it did.
    bool offer(const size_t partner, const double cost) {
        if (cost < this->cost) {
            this->index = partner;
            this->cost = cost;
            return true;
        }
        return false;
    }
};

// Pairs the entries of the chunks one consumer is given.
class ChunkMerger {
public:
    ChunkMerger(bvh::BatchWriter &sink, frontier::BatchWriter &next_run, const Options &options)
        : _sink(sink),
          _next_run(next_run),
          _search_radius(options.search_radius),
          _error_weight(options.error_weight),
          _scene_bounds(options.scene_bounds) {}

    // Merges one chunk, writes what it owns, and returns how many pairs it merged. `chunk` needs a halo of twice the search radius.
    [[nodiscard]]
    alp::Result<uint64_t, io::Error> merge_chunk(const PaddedSpan<const frontier::Entry> chunk) {
        const std::span<const frontier::Entry> window = chunk.span();
        const std::span<const radix::geometry::Aabb<4, double>> bounds = this->extend_bounds(window);
        const std::span<const Nearest> nearest = this->find_nearest(bounds);

        const size_t owned_first = chunk.left_padding();
        const size_t owned_last = owned_first + chunk.size();

        uint64_t merges = 0;
        for (const size_t current : range(owned_first, owned_last)) {
            const std::optional<size_t> partner = nearest[current].index;
            const bool is_pair = partner.has_value() && nearest[partner.value()].index == current;

            if (!is_pair) {
                ALP_TRY(this->_next_run.write(window[current]));
                continue;
            }
            // The chunk holding the left member of a pair is the one that writes it.
            if (partner.value() < current) {
                continue;
            }

            const frontier::Entry &left = window[current];
            const frontier::Entry &right = window[partner.value()];
            ALP_TRY(const frontier::Entry merged, frontier::merge_entries(this->_sink, this->_scene_bounds, left, right));
            ALP_TRY(this->_next_run.write(merged));
            merges++;
        }
        return merges;
    }

private:
    // Extends every entry's bounds by its error.
    [[nodiscard]]
    std::span<const radix::geometry::Aabb<4, double>> extend_bounds(const std::span<const frontier::Entry> entries) {
        this->_bounds.clear();
        std::ranges::transform(entries, std::back_inserter(this->_bounds), [this](const frontier::Entry &e) {
            return build::extend_by_error(e.header, this->_error_weight);
        });
        return this->_bounds;
    }

    // Finds the cheapest partner within the search radius for every entry.
    [[nodiscard]]
    std::span<const Nearest> find_nearest(const std::span<const radix::geometry::Aabb<4, double>> bounds) {
        this->_nearest.assign(bounds.size(), Nearest{});

        // The cost is symmetric, so each pair is measured once and offered to both of its members.
        for (const size_t left : range(bounds.size())) {
            const size_t last = std::min(bounds.size(), left + this->_search_radius + 1);
            for (const size_t right : range(left + 1, last)) {
                const double cost = build::compute_merge_cost(bounds[left], bounds[right]);
                this->_nearest[left].offer(right, cost);
                this->_nearest[right].offer(left, cost);
            }
        }
        return this->_nearest;
    }

    bvh::BatchWriter &_sink;
    frontier::BatchWriter &_next_run;
    size_t _search_radius;
    double _error_weight;
    bvh::Bounds _scene_bounds;
    std::vector<radix::geometry::Aabb<4, double>> _bounds;
    std::vector<Nearest> _nearest;
};

// Splits every region into the entries small enough to merge this round and the ones that sit it out.
template <typename AdmittedSink, typename HeldSink>
class AdmissionGate {
public:
    AdmissionGate(AdmittedSink admitted, HeldSink held, const AdmissionOptions &options)
        : _admitted(std::move(admitted)),
          _held(std::move(held)),
          _options(options) {
        this->_region.reserve(options.region_size);
    }

    // Takes a batch, splitting off every region it completes. Reports whether the admitted sink is still taking entries.
    [[nodiscard]]
    alp::Result<bool, io::Error> push(const std::span<const frontier::Entry> entries) {
        std::span<const frontier::Entry> remaining = entries;
        while (!remaining.empty()) {
            const size_t room = this->_options.region_size - this->_region.size();
            const size_t taken = std::min(room, remaining.size());

            this->_region.append_range(remaining.first(taken));
            remaining = remaining.subspan(taken);

            if (this->_region.size() == this->_options.region_size) {
                ALP_TRY(const bool keep_going, this->drain_region());
                if (!keep_going) {
                    return false;
                }
            }
        }
        return true;
    }

    // Splits the region left at the end of the run.
    [[nodiscard]]
    alp::Result<void, io::Error> flush() {
        if (this->_region.empty()) {
            return {};
        }
        ALP_TRY(this->drain_region());
        return {};
    }

private:
    // Sends the buffered region to both sinks, cut at the limit the region asks for. Reports whether the admitted sink is still taking entries.
    [[nodiscard]]
    alp::Result<bool, io::Error> drain_region() {
        this->_metrics.clear();
        std::ranges::transform(this->_region, std::back_inserter(this->_metrics), [](const frontier::Entry &e) {
            return geometry::compute_mean_extent(e.header.bounds);
        });
        const double limit = compute_admission_limit(this->_metrics, this->_options);

        const auto held = std::ranges::stable_partition(this->_region, [limit](const frontier::Entry &entry) {
            return geometry::compute_mean_extent(entry.header.bounds) <= limit;
        });

        ALP_TRY(const bool keep_going, this->_admitted(std::span<const frontier::Entry>(this->_region.begin(), held.begin())));
        if (!keep_going) {
            return false;
        }
        ALP_TRY(this->_held(std::span<const frontier::Entry>(held.begin(), this->_region.end())));

        this->_region.clear();
        return true;
    }

    std::vector<frontier::Entry> _region;
    std::vector<double> _metrics;
    AdmittedSink _admitted;
    HeldSink _held;
    AdmissionOptions _options;
};

// Reads the run, gives the ring what this round admits and keeps the rest out of it, and returns how many entries it read.
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

// Pairs chunks as they arrive, and returns how many pairs it merged.
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
    const Options &options) {
    EntryRing ring(options.chunk_size, options.chunk_count, 2 * options.search_radius);

    WorkerGroup workers(options.worker_count, [&]() {
        return run_consumer(ring, merged, output, options);
    });

    const WorkerResult read_result = run_producer(input, merged, ring, options);

    uint64_t merges = 0;
    for (const WorkerResult &result : workers.join()) {
        if (!result.has_value()) {
            auto failure_result = result.error().narrow<io::Error>();
            if (failure_result.has_value()) {
                return alp::propagate(std::move(failure_result).value());
            }
            continue;
        }
        merges += result.value();
    }

    if (!read_result.has_value()) {
        auto failure_result = read_result.error().narrow<io::Error>();
        ASSERT(failure_result.has_value(), "the round was cancelled without any worker failing");
        return alp::propagate(std::move(failure_result).value());
    }

    ASSERT(ring.stream_state() != EntryRing::StreamState::Aborted, "the round aborted without any worker failing");
    return read_result.value() - merges;
}

// Opens the runs a round reads and writes.
class RoundLayout {
public:
    explicit RoundLayout(std::filesystem::path root) : _root(std::move(root)) {}

    // The run a round writes, in whatever order its chunks produced it.
    [[nodiscard]]
    frontier::Storage open_merged(const uint32_t round) const {
        return frontier::Storage(this->get_round_path(round) / "merged");
    }

    // The same run in morton order, which is what the next round reads.
    [[nodiscard]]
    frontier::Storage open_sorted(const uint32_t round) const {
        return frontier::Storage(this->get_round_path(round) / "sorted");
    }

private:
    [[nodiscard]]
    std::filesystem::path get_round_path(const uint32_t round) const {
        return this->_root / fmt::format("round-{:04}", round);
    }

    std::filesystem::path _root;
};

} // namespace detail

// Builds the tree by PLOC, pairing nearest neighbours on the frontier round by round until one node is left.
// The frontier must be non-empty and in morton order.
[[nodiscard]]
inline alp::Result<bvh::Reference, io::Error> build(
    frontier::Storage input, bvh::Storage &output,
    const std::filesystem::path &scratch_root, const Options &options) {
    ASSERT(input.size() > 0, "the frontier is empty");

    const auto context = alp::make_context("building a tree from {} to {}", std::filesystem::path(input.directory()), output.directory());

    const detail::RoundLayout layout(scratch_root);
    const frontier::SortOptions sort_options{
        .max_batch_bytes = options.max_batch_bytes,
        .max_resident_bytes = options.max_resident_bytes,
        .prefix_bits = options.sort_prefix_bits,
    };

    frontier::Storage current = std::move(input);
    uint64_t previous = std::numeric_limits<uint64_t>::max();
    for (uint32_t round = 0;; round++) {
        frontier::Storage merged = layout.open_merged(round);
        ALP_TRY_CONTEXT(
            const uint64_t remaining,
            alp::with_context(detail::run_round(current, merged, output, options), alp::make_context("merging round {}", round)),
            context);
        ASSERT(remaining < previous, "a ploc round left the frontier no smaller than it found it");
        previous = remaining;

        ALP_TRY_CONTEXT(merged.save_index(), context);
        LOG_DEBUG("ploc round {} left {} entries on the frontier", round, remaining);

        frontier::Storage sorted = layout.open_sorted(round);
        ALP_TRY_CONTEXT(frontier::sort_by_morton(merged, sorted, sort_options), context);
        ALP_TRY_CONTEXT(sorted.save_index(), context);

        if (remaining == 1) {
            ALP_TRY_CONTEXT(const frontier::Batch batch, sorted.read(0), context);
            ALP_TRY_CONTEXT(output.save_index(), context);
            return batch.entries.front().ref;
        }

        current = std::move(sorted);
    }
}

} // namespace ploc
