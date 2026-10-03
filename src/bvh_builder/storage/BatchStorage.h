#pragma once

#include <cstdint>
#include <expected>
#include <filesystem>
#include <utility>

#include <fmt/format.h>
#include <libassert/assert.hpp>

#include "storage/BaseStorage.h"
#include "io/Error.h"
#include "error/Result.h"
#include "error/try.h"

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

    using Base::directory;
    using Base::remove;
    using Base::save_index;
    using Base::size;

    [[nodiscard]]
    alp::Result<uint32_t, io::Error> write(const Batch &batch) {
        const uint32_t index = this->allocate_index();
        ALP_TRY(this->write(index, batch));
        this->mark_dirty();
        return index;
    }

    [[nodiscard]]
    alp::Result<uint32_t, io::Error> write(const uint32_t index, const Batch &batch) {
        ASSERT(index < this->size());
        ALP_TRY_CONTEXT(io::write_to_path(batch, this->get_batch_path(index)), fmt::format("writing batch {}", index));
        return index;
    }

    [[nodiscard]]
    alp::Result<Batch, io::Error> read(const uint32_t index) const {
        ASSERT(index < this->size());
        ALP_TRY_CONTEXT(Batch batch, io::read_from_path<Batch>(this->get_batch_path(index)), fmt::format("reading batch {}", index));
        return batch;
    }
};
