#pragma once

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <utility>

#include <fmt/format.h>
#include <tl/expected.hpp>
#include <zpp_bits.h>

#include "io/Error.h"
#include "io/serialize.h"
#include "log.h"

constexpr size_t default_max_batch_bytes = 64 << 20;

// Directory of positionally named batch files.
template <typename T>
class BatchStore_ {
public:
    struct Index {
        uint32_t batch_count = 0;
    };

    explicit BatchStore_(std::filesystem::path directory)
        : _directory(std::move(directory)) {
        const auto stored = io::read_from_path<Index>(this->index_path());
        if (stored) {
            this->_next.store(stored.value().batch_count);
        }
    }

    BatchStore_(const BatchStore_<T> &) = delete;
    BatchStore_<T> &operator=(const BatchStore_<T> &) = delete;

    BatchStore_(BatchStore_<T> &&other) noexcept
        : _directory(std::move(other._directory)),
          _next(other._next.load()),
          _dirty(std::exchange(other._dirty, false)) {}

    BatchStore_<T> &operator=(BatchStore_<T> &&other) noexcept {
        this->_directory = std::move(other._directory);
        this->_next.store(other._next.load());
        this->_dirty = std::exchange(other._dirty, false);
        return *this;
    }

    ~BatchStore_() {
        if (!this->_dirty) {
            return;
        }

        LOG_WARN("Index was not saved upon BatchStore_ destruction, use save().");
        const auto result = this->save();
        if (!result) {
            LOG_ERROR("Failed to save index of {}: {}", this->_directory.string(), result.error());
        }
    }

    // Reserves the next index and returns an empty batch carrying it.
    T create_batch() {
        this->_dirty = true;

        T batch;
        batch.index = this->_next.fetch_add(1);
        return batch;
    }

    tl::expected<void, io::Error> write(const T &batch) const {
        DEBUG_ASSERT(batch.index < this->_next.load());
        return io::write_to_path(batch, this->path_of(batch.index));
    }

    tl::expected<T, io::Error> read(const uint32_t index) const {
        auto batch = io::read_from_path<T>(this->path_of(index));
        if (batch) {
            DEBUG_ASSERT(batch.value().index == index);
        }
        return batch;
    }

    tl::expected<void, io::Error> save() const {
        const Index index{.batch_count = this->_next.load()};
        auto result = io::write_to_path(index, this->index_path());
        if (result) {
            this->_dirty = false;
        }
        return result;
    }

    uint32_t size() const {
        return this->_next.load();
    }

private:
    std::filesystem::path path_of(const uint32_t index) const {
        return this->_directory / fmt::format("{:08}.batch", index);
    }

    std::filesystem::path index_path() const {
        return this->_directory / "index";
    }

    std::filesystem::path _directory;
    std::atomic<uint32_t> _next = 0;
    mutable bool _dirty = false;
};