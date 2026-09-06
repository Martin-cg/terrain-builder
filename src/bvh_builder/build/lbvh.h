#pragma once

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <span>
#include <vector>

#include <libassert/assert.hpp>
#include <tl/expected.hpp>

#include "io/Error.h"
#include "numeric/wide_integer.h"
#include "try.h"

#include "storage/batch_limits.h"
#include "bvh/BatchWriter.h"
#include "bvh/Storage.h"
#include "bvh/layout.h"
#include "frontier/Storage.h"
#include "frontier/layout.h"
#include "frontier/merge_entry.h"
#include "frontier/traverse.h"
#include "morton/code.h"

namespace lbvh {

struct Options {
    size_t max_batch_bytes = default_max_batch_bytes;
};

namespace detail {

// Counts the leading bits both codes share.
template <typename Code>
[[nodiscard]]
uint32_t count_common_prefix_bits(const Code &a, const Code &b) {
    const Code difference = a ^ b;
    if (difference == Code{0}) {
        return morton::code_bits;
    }

    // Only the matching branch may be instantiated; the other does not compile for this type.
    if constexpr (is_wide_integer_v<Code>) {
        return morton::code_bits - 1 - math::wide_integer::msb(difference);
    } else {
        return std::countl_zero(difference);
    }
}

// Keeps the code's leading `bits` bits and clears the rest.
[[nodiscard]]
inline morton::Code extract_prefix(const morton::Code &code, const uint32_t bits) {
    if (bits == 0) {
        return morton::Code{0};
    }

    const morton::Code mask = std::numeric_limits<morton::Code>::max() << (morton::code_bits - bits);
    return code & mask;
}

// Finds where the run divides at the highest bit its codes disagree on, or in half when they
// all agree. Never leaves either side empty.
[[nodiscard]]
inline size_t find_split(const std::span<const frontier::Entry> entries) {
    DEBUG_ASSERT(entries.size() >= 2);

    const uint32_t shared_bits = count_common_prefix_bits(entries.front().morton, entries.back().morton);
    if (shared_bits == morton::code_bits) {
        return entries.size() / 2;
    }

    const morton::Code prefix = extract_prefix(entries.front().morton, shared_bits);
    const morton::Code target = prefix | (morton::Code{1} << (morton::code_bits - shared_bits - 1));
    const auto first_right = std::ranges::lower_bound(entries, target, {}, &frontier::Entry::morton);
    return static_cast<size_t>(std::distance(entries.begin(), first_right));
}

// Writes the subtree over `entries` and returns its root, at the first code of the run.
// `entries` must be non-empty and in morton order.
[[nodiscard]]
inline tl::expected<frontier::Entry, io::Error> build_subtree(
    const std::span<const frontier::Entry> entries, bvh::BatchWriter &sink) {
    ASSERT(!entries.empty());

    if (entries.size() == 1) {
        return entries.front();
    }

    const size_t split = find_split(entries);
    TRY(const frontier::Entry left, build_subtree(entries.first(split), sink));
    TRY(const frontier::Entry right, build_subtree(entries.subspan(split), sink));
    return frontier::merge_entries(sink, left.morton, left, right);
}

} // namespace detail

// Writes the tree over the run through `sink` and returns its root. The caller owns the writer.
// The run must be non-empty and in morton order.
[[nodiscard]]
inline tl::expected<frontier::Entry, io::Error> build(
    const frontier::Storage &input, bvh::BatchWriter &sink) {
    ASSERT(input.size() > 0);

    std::vector<frontier::Entry> batch_roots;
    batch_roots.reserve(input.size());

    TRY(frontier::traverse_batches(input, [&](const frontier::Batch &batch) -> tl::expected<void, io::Error> {
        TRY(const frontier::Entry root, detail::build_subtree(batch.entries, sink));
        batch_roots.push_back(root);
        return {};
    }));

    return detail::build_subtree(batch_roots, sink);
}

// Writes the tree over the run into `output`, saves its index, and returns the root reference.
// The run must be non-empty and in morton order.
[[nodiscard]]
inline tl::expected<bvh::Reference, io::Error> build(
    const frontier::Storage &input, bvh::Storage &output, const Options &options) {
    bvh::BatchWriter sink(output, options.max_batch_bytes);

    TRY(const frontier::Entry root, build(input, sink));
    TRY(sink.flush());
    TRY(output.save_index());
    return root.ref;
}

} // namespace lbvh
