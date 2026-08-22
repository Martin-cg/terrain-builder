#pragma once

#include <filesystem>
#include <utility>

#include <tl/expected.hpp>

#include "BaseStorage.h"
#include "frontier/layout.h"
#include "io/Error.h"
#include "try.h"

namespace frontier {

class Storage : private BaseStorage {
    using Base = BaseStorage;

public:
    explicit Storage(std::filesystem::path directory)
        : Base(std::move(directory)) {}

    Storage(const Storage &) = delete;
    Storage &operator=(const Storage &) = delete;

    Storage(Storage &&) noexcept = default;
    Storage &operator=(Storage &&) noexcept = default;

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
        TRY(io::write_to_path(batch, this->batch_path(index)));
        return index;
    }

    tl::expected<Batch, io::Error> read(const uint32_t index) const {
        ASSERT(index < this->size());
        return io::read_from_path<Batch>(this->batch_path(index));
    }
};
}
