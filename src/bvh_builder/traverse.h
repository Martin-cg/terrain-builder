#pragma once

#include <cstdint>
#include <utility>

#include <tl/expected.hpp>

#include "Range.h"
#include "io/Error.h"

#include "frontier.h"
#include "storage.h"

namespace frontier {

// Streams the run's batches one at a time, visiting every entry in order. Resident set is one batch, so a worker calls this on its own range and the halo is its own business.
template <typename Visitor>
tl::expected<void, io::Error> traverse(
    const RunStorage &run, Visitor &&visitor, const AnyRange<uint32_t> batches = RangeFull{}) {
    const Range<uint32_t> range = batches.to_range(run.size());

    for (uint32_t index = range.start; index < range.end; index++) {
        const auto result = run.read(index);
        if (!result) {
            return tl::unexpected(result.error());
        }

        const auto& batch = result.value();
        for (const Entry &entry : batch.entries) {
            visitor(entry);
        }
    }
    return {};
}

}
