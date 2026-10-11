#pragma once

#include <cstdint>
#include <filesystem>
#include <limits>
#include <utility>

#include <libassert/assert.hpp>

#include "error/Result.h"
#include "error/try.h"
#include "error/utils.h"
#include "io/DirectoryNotEmpty.h"
#include "io/Error.h"
#include "io/ScratchDirectory.h"
#include "log.h"

#include "bvh/Storage.h"
#include "bvh/layout.h"
#include "frontier/Storage.h"
#include "frontier/layout.h"
#include "gated_ploc/RoundLayout.h"
#include "gated_ploc/options.h"
#include "gated_ploc/run_round.h"
#include "sort/options.h"
#include "sort/sort_by_morton.h"

namespace gated_ploc {

// Builds the tree by gated PLOC, pairing nearest neighbours on the frontier round by round until one node is left.
// The frontier must be non-empty and in morton order, and is removed once it has been read.
[[nodiscard]]
inline alp::Result<bvh::Reference, io::Error, io::DirectoryNotEmpty> build(
    frontier::Storage input, bvh::Storage &output,
    const std::filesystem::path &scratch_root, const Options &options) {
    ASSERT(input.size() > 0, "the frontier is empty");

    const auto context = alp::make_context("building a tree from {} to {}", std::filesystem::path(input.directory()), output.directory());

    const detail::RoundLayout layout(scratch_root);
    const sort::Options sort_options{
        .max_batch_bytes = options.max_batch_bytes,
        .max_resident_bytes = options.max_resident_bytes,
    };

    detail::EntryRing ring(options.chunk_size, options.chunk_count, 2 * options.search_radius);

    frontier::Storage current = std::move(input);
    uint64_t previous = std::numeric_limits<uint64_t>::max();
    for (uint32_t round = 0;; round++) {
        ALP_TRY_CONTEXT(frontier::Storage merged, layout.create_merge_scratch(round), context);
        ALP_TRY_CONTEXT(
            const uint64_t remaining,
            alp::with_context(detail::run_round(current, merged, output, ring, options), alp::make_context("merging round {}", round)),
            context);
        ASSERT(remaining < previous, "a gated ploc round left the frontier no smaller than it found it");
        previous = remaining;

        ALP_TRY_CONTEXT(merged.save_index(), context);
        LOG_DEBUG("gated ploc round {} left {} entries on the frontier", round, remaining);
        ALP_TRY_CONTEXT(std::move(current).remove(), context);

        ALP_TRY_CONTEXT(frontier::Storage sorted, layout.create_sort_scratch(round), context);
        ALP_TRY_CONTEXT(const io::ScratchDirectory sort_runs, layout.create_sort_runs_scratch(round), context);
        ALP_TRY_CONTEXT(sort::sort_by_morton(merged, sorted, sort_runs.path(), sort_options), context);
        ALP_TRY_CONTEXT(std::move(merged).remove(), context);

        if (remaining == 1) {
            ALP_TRY_CONTEXT(const frontier::Batch batch, sorted.read(0), context);
            ALP_TRY_CONTEXT(std::move(sorted).remove(), context);
            ALP_TRY_CONTEXT(output.save_index(), context);
            return batch.entries.front().ref;
        }

        current = std::move(sorted);
    }
}

} // namespace gated_ploc
