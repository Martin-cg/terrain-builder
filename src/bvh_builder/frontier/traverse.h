#pragma once

#include <cstdint>
#include <utility>

#include <tl/expected.hpp>

#include "Range.h"
#include "io/Error.h"
#include "frontier/layout.h"
#include "frontier/Storage.h"
#include "try.h"
#include "visitor.h"

namespace frontier {

// Streams the run's batches one at a time.
template <typename F>
tl::expected<void, io::Error> traverse_batches(const Storage &run, F &&f, const AnyRange<uint32_t> batches = RangeFull{}) {
    visitor::FallibleVisitor<io::Error, F &> visitor(f);

    const Range<uint32_t> range = batches.to_range(run.size());

    for (uint32_t index = range.start; index < range.end; index++) {
        TRY(const Batch &batch, run.read(index));
        TRY(const bool keep_going, visitor(batch));
        if (!keep_going) {
            break;
        }
    }

    return {};
}

// Streams the run's batches one at a time, visiting every entry in order.
template <typename F>
tl::expected<void, io::Error> traverse_entries(const Storage &run, F &&f, const AnyRange<uint32_t> batches = RangeFull{}) {
    visitor::FallibleVisitor<io::Error, F &> visitor(f);

    return traverse_batches(run, [&](const Batch &batch) -> tl::expected<bool, io::Error> {
        for (const Entry &entry : batch.entries) {
            TRY(const bool keep_going, visitor(entry));
            if (!keep_going) {
                return false;
            }
        }
        return true;
    }, batches);
}

}
