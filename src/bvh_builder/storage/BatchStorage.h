#pragma once

#include <cstdint>
#include <filesystem>
#include <utility>

#include <tl/expected.hpp>

#include "storage/BaseStorage.h"
#include "io/Error.h"
#include "try.h"

// A run of batches, each written whole and read back by the index it was written at.
template <typename BatchType>
class BatchStorage : private BaseStorage {
    using Base = BaseStorage;

public:
    using Batch = BatchType;

    explicit BatchStorage(std::filesystem::path directory)
        : Base(std::move(directory)) {}

    BatchStorage(const BatchStorage &) = delete;
    BatchStorage &operator=(const BatchStorage &) = delete;

    BatchStorage(BatchStorage &&) noexcept = default;
    BatchStorage &operator=(BatchStorage &&) noexcept = default;

    using Base::save_index;
    using Base::size;

    tl::expected<uint32_t, io::Error> write(const Batch &batch) {
        const uint32_t index = this->allocate_index();
        TRY(this->write(index, batch));
        this->mark_dirty();
        return index;
    }

    tl::expected<uint32_t, io::Error> write(const uint32_t index, const Batch &batch) {
        ASSERT(index < this->size());
        TRY(io::write_to_path(batch, this->get_batch_path(index)));
        return index;
    }

    tl::expected<Batch, io::Error> read(const uint32_t index) const {
        ASSERT(index < this->size());
        return io::read_from_path<Batch>(this->get_batch_path(index));
    }
};
