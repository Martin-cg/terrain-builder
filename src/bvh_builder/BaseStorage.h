#pragma once

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <utility>

#include <fmt/format.h>
#include <tl/expected.hpp>

#include "io/Error.h"
#include "io/serialize.h"
#include "log.h"

class BaseStorage {
public:
    struct Index {
        uint32_t batch_count = 0;
    };

    explicit BaseStorage(std::filesystem::path directory) : _directory(std::move(directory)) {
        const auto stored = io::read_from_path<Index>(this->index_path());
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

    tl::expected<void, io::Error> save_index() const {
        const Index index{
            .batch_count = this->_next.load(),
        };

        auto result = io::write_to_path(index, this->index_path());
        if (result) {
            this->_dirty = false;
        }

        return result;
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

    std::filesystem::path batch_path(const uint32_t index) const {
        return this->_directory / fmt::format("batch-{:08}.bin", index);
    }

private:
    std::filesystem::path index_path() const {
        return this->_directory / "index.bin";
    }

    std::filesystem::path _directory;
    std::atomic<uint32_t> _next = 0;
    mutable std::atomic<bool> _dirty = false;
};

