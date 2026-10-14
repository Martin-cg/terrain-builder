#pragma once

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <span>
#include <utility>
#include <vector>

#include <libassert/assert.hpp>

#include "error/Result.h"
#include "error/try.h"
#include "geometry/bounds.h"
#include "io/Error.h"

#include "frontier/layout.h"
#include "gated_ploc/options.h"

namespace gated_ploc {

namespace detail {

// Computes the extent up to which a region with these `metrics` admits, reordering them. `metrics` must not be empty.
[[nodiscard]]
inline double compute_admission_limit(std::vector<double> &metrics, const AdmissionOptions &options) {
    DEBUG_ASSERT(!metrics.empty());

    // Enough for the two smallest nodes to find each other.
    const auto count = static_cast<std::ptrdiff_t>(metrics.size());
    const std::ptrdiff_t second = std::min<std::ptrdiff_t>(1, count - 1);
    std::ranges::nth_element(metrics, metrics.begin() + second);
    const double smallest_pair = metrics[second];

    // As much more around them as the tolerance allows.
    const auto index = static_cast<std::ptrdiff_t>(options.quantile * (count - 1));
    std::ranges::nth_element(metrics, metrics.begin() + index);
    return std::max(smallest_pair, metrics[index] * options.tolerance);
}

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
            return geometry::compute_mean_extent(e.header.tight_bounds);
        });
        const double limit = compute_admission_limit(this->_metrics, this->_options);

        const auto held = std::ranges::stable_partition(this->_region, [limit](const frontier::Entry &entry) {
            return geometry::compute_mean_extent(entry.header.tight_bounds) <= limit;
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

} // namespace detail

} // namespace gated_ploc
