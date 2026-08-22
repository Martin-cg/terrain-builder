#pragma once

#include <filesystem>
#include <utility>

#include <tl/expected.hpp>

#include "BaseStorage.h"
#include "bvh/layout.h"
#include "io/Error.h"
#include "log.h"

namespace bvh {

class Storage : private BaseStorage {
    using Base = BaseStorage;

public:
    explicit Storage(std::filesystem::path directory) : Base(std::move(directory)) {}

    Storage(const Storage &) = delete;
    Storage &operator=(const Storage &) = delete;

    Storage(Storage &&) noexcept = default;
    Storage &operator=(Storage &&) noexcept = default;

    using Base::save_index;
    using Base::size;

    Batch create_batch() {
        const uint32_t index = this->allocate_index();
        return Batch(index);
    }

    tl::expected<void, io::Error> write(const Batch &batch) const {
        ASSERT(batch.index() < this->size());
        this->mark_dirty();
        return io::write_to_path(batch, this->batch_path(batch.index()));
    }

    tl::expected<Batch, io::Error> read(const uint32_t index) const {
        auto result = io::read_from_path<Batch>(this->batch_path(index));
        if (result) {
            ASSERT(result->index() == index);
        }
        return result;
    }
};

} // namespace bvh
