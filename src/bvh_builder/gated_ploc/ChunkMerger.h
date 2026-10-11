#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <optional>
#include <span>
#include <vector>

#include <radix/geometry.h>

#include "containers/ChunkedRingBuffer.h"
#include "error/Result.h"
#include "error/try.h"
#include "io/Error.h"
#include "range_utils.h"

#include "build/merge_cost.h"
#include "bvh/BatchWriter.h"
#include "bvh/layout.h"
#include "frontier/BatchWriter.h"
#include "frontier/layout.h"
#include "frontier/merge_entry.h"
#include "gated_ploc/options.h"

namespace gated_ploc {

namespace detail {

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

} // namespace detail

} // namespace gated_ploc
