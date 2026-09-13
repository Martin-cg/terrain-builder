#pragma once

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <utility>

#include <fmt/format.h>
#include <tl/expected.hpp>

#include "error/Result.h"
#include "error/format.h"
#include "error/try.h"
#include "io/Error.h"
#include "io/serialize.h"
#include "log.h"

class BaseStorage {
public:
    struct Index {
        uint32_t batch_count = 0;
    };

    explicit BaseStorage(std::filesystem::path directory) : _directory(std::move(directory)) {
        const auto stored = io::read_from_path<Index>(this->get_index_path());
        if (stored) {
            this->_next.store(stored->batch_count);
        }
    }

    BaseStorage(const BaseStorage &) = delete;
    BaseStorage &operator=(const BaseStorage &) = delete;

    BaseStorage(BaseStorage &&other) noexcept
        : _directory(std::move(other._directory)),
          _next(other._next.load()),
          _dirty(other._dirty.exchange(false)) {}

    BaseStorage &operator=(BaseStorage &&other) noexcept {
        this->_directory = std::move(other._directory);
        this->_next.store(other._next.load());
        this->_dirty.store(other._dirty.exchange(false));
        return *this;
    }

    ~BaseStorage() {
        if (!this->_dirty.load()) {
            return;
        }

        LOG_WARN("Index was not saved upon storage destruction, use save_index().");

        const auto result = this->save_index();
        if (!result) {
            LOG_ERROR("Failed to save index of {}: {}", this->_directory.string(), result.error());
        }
    }

    [[nodiscard]]
    alp::Result<void, io::Error> save_index() const {
        const Index index{
            .batch_count = this->_next.load(),
        };

        ALP_TRY(io::write_to_path(index, this->get_index_path()));
        this->_dirty = false;
        return {};
    }

    uint32_t size() const {
        return this->_next.load();
    }

protected:
    uint32_t allocate_index() {
        return this->_next.fetch_add(1);
    }

    void mark_dirty() const {
        this->_dirty = true;
    }

    std::filesystem::path get_batch_path(const uint32_t index) const {
        return this->_directory / fmt::format("batch-{:08}.bin", index);
    }

private:
    std::filesystem::path get_index_path() const {
        return this->_directory / "index.bin";
    }

    std::filesystem::path _directory;
    std::atomic<uint32_t> _next = 0;
    mutable std::atomic<bool> _dirty = false;
};

