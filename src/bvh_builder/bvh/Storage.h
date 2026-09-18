#pragma once

#include <filesystem>
#include <utility>

#include <fmt/format.h>
#include <libassert/assert.hpp>
#include <tl/expected.hpp>

#include "storage/BaseStorage.h"
#include "bvh/layout.h"
#include "error/Result.h"
#include "error/try.h"
#include "io/Error.h"
#include "log.h"

namespace bvh {

class Storage : private BaseStorage {
    using Base = BaseStorage;

public:
    using Batch = bvh::Batch;

    explicit Storage(std::filesystem::path directory) : Base(std::move(directory)) {}

    Storage(const Storage &) = delete;
    Storage &operator=(const Storage &) = delete;

    Storage(Storage &&) noexcept = default;
    Storage &operator=(Storage &&) noexcept = default;

    using Base::directory;
    using Base::save_index;
    using Base::size;

    Batch create_batch() {
        const uint32_t index = this->allocate_index();
        return Batch(index);
    }

    [[nodiscard]]
    alp::Result<void, io::Error> write(const Batch &batch) const {
        ASSERT(batch.index() < this->size());
        this->mark_dirty();
        ALP_TRY_CONTEXT(io::write_to_path(batch, this->get_batch_path(batch.index())), fmt::format("writing batch {}", batch.index()));
        return {};
    }

    [[nodiscard]]
    alp::Result<Batch, io::Error> read(const uint32_t index) const {
        ALP_TRY_CONTEXT(Batch batch, io::read_from_path<Batch>(this->get_batch_path(index)), fmt::format("reading batch {}", index));
        ASSERT(batch.index() == index);
        return batch;
    }
};

} // namespace bvh
