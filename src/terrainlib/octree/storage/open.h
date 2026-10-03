#pragma once

#include <filesystem>
#include <memory>
#include <optional>

#include "error/Error.h"
#include "error/Result.h"
#include "io/Error.h"
#include "octree/disk/layout/UnknownLayoutStrategy.h"
#include "octree/disk/layout/strategy/Default.h"
#include "octree/storage/IndexedStorage.h"
#include "octree/storage/Storage.h"
#include "octree/storage/codec/Codec.h"
#include "octree/storage/defaults.h"

namespace octree {

struct OpenOptions {
    std::unique_ptr<disk::layout::Strategy> default_layout_strategy = {};
    std::optional<std::string> preferred_extension_with_dot = {};
};

// An error from opening a storage.
using OpenError = alp::Error<io::FileError, io::SerdeError, disk::layout::UnknownLayoutStrategy>;

template <typename T = DefaultT, CodecFor<T> Codec = DefaultCodecFor<T>>
[[nodiscard]]
alp::Result<IndexedStorage_<T, Codec>, OpenError> open_index(const std::filesystem::path &index_path);
template <typename T = DefaultT, CodecFor<T> Codec = DefaultCodecFor<T>>
[[nodiscard]]
alp::Result<Storage_<T, Codec>, OpenError> open_folder(
    const std::filesystem::path &base_path,
    const bool create_index = false,
    OpenOptions options = {});
template <typename T = DefaultT, CodecFor<T> Codec = DefaultCodecFor<T>>
[[nodiscard]]
alp::Result<IndexedStorage_<T, Codec>, OpenError> open_folder_indexed(
    const std::filesystem::path &base_path,
    OpenOptions options = {});
}

#include "open.inl"

