#pragma once

#include <filesystem>

#include <libassert/assert.hpp>

#include "error/Result.h"
#include "error/try.h"
#include "io/Error.h"
#include "io/FileError.h"
#include "log.h"
#include "octree/Id.h"
#include "octree/IndexMap.h"
#include "octree/disk/Layout.h"
#include "octree/storage/Storage.h"
#include "octree/storage/codec/Codec.h"
#include "octree/storage/defaults.h"
#include "octree/storage/helpers.h"

namespace octree {

template <typename T = DefaultT, CodecFor<T> Codec = DefaultCodecFor<T>>
class IndexedStorage_ : public Storage_<T, Codec> {
public:
    // Turns `inner` into an indexed storage, building its index from the stored nodes if it has none.
    [[nodiscard]]
    static alp::Result<IndexedStorage_, io::FileError> from_storage(Storage_<T, Codec> inner) {
        ALP_TRY(inner.ensure_indexed());
        return IndexedStorage_(std::move(inner));
    }
    explicit IndexedStorage_(RawStorage_<T, Codec> inner, IndexMap map) noexcept
        : Storage_<T, Codec>(std::move(inner), std::move(map)) {
        DEBUG_ASSERT(this->is_indexed());
    }

    IndexedStorage_<T, Codec> &operator=(const IndexedStorage_<T, Codec> &) = delete;
    IndexedStorage_(const IndexedStorage_<T, Codec> &) = delete;
    IndexedStorage_(IndexedStorage_<T, Codec> &&) = default;
    IndexedStorage_<T, Codec> &operator=(IndexedStorage_<T, Codec> &&) = default;

    ~IndexedStorage_() override {
        if (this->is_index_dirty()) {
            LOG_WARN("Index was not saved upon IndexedStorage_ destruction, use save_index().");
        }
    }

    const IndexMap& index() const noexcept {
        const auto index_opt = Storage_<T, Codec>::index();
        return *ASSERT_VAL(index_opt);
    }
    // Rebuilds the index from the stored nodes; keeps the old index on failure.
    [[nodiscard]]
    alp::Result<void, io::FileError> update_index() noexcept {
        return Storage_<T, Codec>::update_index();
    }
    [[nodiscard]]
    io::Result<void> save_index() const {
        if (!this->is_index_dirty()) {
            return {};
        }

        auto result = helpers::save_index_map(this->index(), this->layout());
        if (result.has_value()) {
            this->set_index_dirty(false);
        }
        return result;
    }

private:
    explicit IndexedStorage_(Storage_<T, Codec> inner) noexcept
        : Storage_<T, Codec>(std::move(inner)) {
        ASSERT(this->is_indexed());
    }

    IndexMap& index_mut() noexcept {
        std::optional<IndexMap> &index_opt = Storage_<T, Codec>::index_mut();
        return *ASSERT_VAL(index_opt);
    }
};

} // namespace octree
