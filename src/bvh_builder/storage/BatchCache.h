#pragma once

#include <cstdint>
#include <functional>
#include <utility>

#include <libassert/assert.hpp>
#include <tl/expected.hpp>

#include "io/Error.h"
#include "try.h"

#include "storage/BoundedCache.h"

// A cache for batches in a storage. Uses an LRU policy for eviction.
template <typename Storage>
class BatchCache {
public:
    using Batch = typename Storage::Batch;

    BatchCache(const Storage &input, const uint32_t max_resident_batches) : _input(input), _cache(max_resident_batches) {
        ASSERT(max_resident_batches > 0, "a cache that holds nothing cannot hand out a batch");
    }

    // The batch at `index`, read if it is not resident, valid until the next read.
    tl::expected<std::reference_wrapper<const Batch>, io::Error> read_batch(const uint32_t index) {
        return this->_cache.get_or_try_insert_with(index, [&]() -> tl::expected<typename Cache::Entry, io::Error> {
            TRY(Batch batch, this->_input.read(index));
            // Every batch counts the same, so the budget is a number of batches.
            return typename Cache::Entry{.value = std::move(batch), .cost = 1};
        });
    }

    // How many batches are resident.
    uint32_t count_resident() const {
        return this->_cache.count_resident();
    }

    // How many reads had to go to disk.
    uint32_t count_misses() const {
        return this->_cache.count_misses();
    }

private:
    using Cache = BoundedCache<uint32_t, Batch>;

    const Storage &_input;
    Cache _cache;
};