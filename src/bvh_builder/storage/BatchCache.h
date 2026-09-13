#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <list>
#include <unordered_map>
#include <utility>

#include <libassert/assert.hpp>
#include <tl/expected.hpp>

#include "io/Error.h"
#include "error/Result.h"
#include "error/try.h"

// A cache for batches in a storage. Uses an LRU policy for eviction.
template <typename Storage>
class BatchCache {
public:
    using Batch = typename Storage::Batch;

    BatchCache(const Storage &input, const uint32_t max_resident_batches) : _input(input), _max_resident_batches(max_resident_batches) {
        ASSERT(max_resident_batches > 0, "a cache that holds nothing cannot hand out a batch");
    }

    // The batch at `index`, read if it is not resident, valid until the next read.
    [[nodiscard]]
    alp::Result<std::reference_wrapper<const Batch>, io::Error> read_batch(const uint32_t index) {
        const auto resident = this->_resident.find(index);
        if (resident != this->_resident.end()) {
            this->_usage.splice(this->_usage.begin(), this->_usage, resident->second.second);
            return std::cref(resident->second.first);
        }

        ALP_TRY(Batch batch, this->_input.read(index));
        this->_misses++;

        // The entry goes in as the most recently used, so eviction can never take the one just read.
        this->_usage.push_front(index);
        const auto admitted = this->_resident.emplace(index, std::pair{std::move(batch), this->_usage.begin()}).first;
        this->evict_until_within_budget();
        return std::cref(admitted->second.first);
    }

    // How many batches are resident.
    uint32_t count_resident() const {
        return static_cast<uint32_t>(this->_resident.size());
    }

    // How many reads had to go to disk.
    uint32_t count_misses() const {
        return this->_misses;
    }

private:
    // The indices of the resident batches, most recently used first.
    using Usage = std::list<uint32_t>;

    // Evicts batches until the resident set is within budget.
    void evict_until_within_budget() {
        while (this->_resident.size() > this->_max_resident_batches) {
            this->_resident.erase(this->_usage.back());
            this->_usage.pop_back();
        }
    }

    const Storage &_input;
    uint32_t _max_resident_batches;
    uint32_t _misses = 0;
    Usage _usage;
    std::unordered_map<uint32_t, std::pair<Batch, Usage::iterator>> _resident;
};