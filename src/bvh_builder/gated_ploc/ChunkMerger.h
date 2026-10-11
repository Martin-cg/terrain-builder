#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "containers/ChunkedRingBuffer.h"
#include "error/Result.h"
#include "error/try.h"
#include "io/Error.h"
#include "range_utils.h"

#include "bvh/BatchWriter.h"
#include "bvh/layout.h"
#include "frontier/BatchWriter.h"
#include "frontier/layout.h"
#include "frontier/merge_entry.h"
#include "gated_ploc/options.h"
#include "ploc/NearestSearch.h"

namespace gated_ploc {

namespace detail {

// Pairs the entries of the chunks one consumer is given.
class ChunkMerger {
public:
    ChunkMerger(bvh::BatchWriter &sink, frontier::BatchWriter &next_run, const Options &options)
        : _sink(sink),
          _next_run(next_run),
          _search(options.search_radius, options.error_weight),
          _scene_bounds(options.scene_bounds) {}

    // Merges one chunk, writes what it owns, and returns how many pairs it merged. `chunk` needs a halo of twice the search radius.
    [[nodiscard]]
    alp::Result<uint64_t, io::Error> merge_chunk(const PaddedSpan<const frontier::Entry> chunk) {
        const std::span<const frontier::Entry> window = chunk.span();
        const std::span<const ploc::Nearest> nearest = this->_search.find_nearest(window);

        const size_t owned_first = chunk.left_padding();
        const size_t owned_last = owned_first + chunk.size();

        uint64_t merges = 0;
        for (const size_t current : range(owned_first, owned_last)) {
            const std::optional<size_t> partner_opt = ploc::find_mutual_partner(nearest, current);
            if (!partner_opt.has_value()) {
                ALP_TRY(this->_next_run.write(window[current]));
                continue;
            }
            // The chunk holding the left member of a pair is the one that writes it.
            if (partner_opt.value() < current) {
                continue;
            }

            const frontier::Entry &left = window[current];
            const frontier::Entry &right = window[partner_opt.value()];
            ALP_TRY(const frontier::Entry merged, frontier::merge_entries(this->_sink, this->_scene_bounds, left, right));
            ALP_TRY(this->_next_run.write(merged));
            merges++;
        }
        return merges;
    }

private:
    bvh::BatchWriter &_sink;
    frontier::BatchWriter &_next_run;
    ploc::NearestSearch _search;
    bvh::Bounds _scene_bounds;
};

} // namespace detail

} // namespace gated_ploc
