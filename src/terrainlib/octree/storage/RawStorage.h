#pragma once

#include <filesystem>
#include <string>
#include <system_error>

#include <fmt/format.h>
#include <tl/expected.hpp>

#include "error/Error.h"
#include "error/try.h"
#include "error/utils.h"
#include "io/FileError.h"
#include "io/utils.h"
#include "octree/Id.h"
#include "octree/disk/Layout.h"
#include "octree/storage/codec/Codec.h"
#include "octree/storage/defaults.h"

namespace octree {

template <typename T = DefaultT, CodecFor<T> Codec = DefaultCodecFor<T>>
class RawStorage_ {
public:
    using value_type = T;
    using codec_type = Codec;
    using load_error = typename Codec::load_error;
    using save_error = typename Codec::save_error;
    using copy_error = alp::Error<io::FileError, load_error, save_error>;

    explicit RawStorage_(disk::Layout layout) noexcept : _layout(std::move(layout)) {}

    ~RawStorage_() = default;
    RawStorage_ &operator=(const RawStorage_ &) = delete;
    RawStorage_(const RawStorage_ &) = delete;
    RawStorage_(RawStorage_ &&) = default;
    RawStorage_ &operator=(RawStorage_ &&) = default;
    
    tl::expected<T, load_error> load(const Id &id) const {
        const auto path = this->path_for(id);
        ALP_TRY_CONTEXT(T node, Codec::load_from_path(path), fmt::format("loading node {}", id));
        return node;
    }

    tl::expected<void, save_error> save(const Id &id, const T &node) const {
        const auto path = this->path_for(id);
        ALP_TRY_CONTEXT(Codec::save_to_path(node, path), fmt::format("saving node {}", id));
        return {};
    }

    tl::expected<void, copy_error> copy_to(const Id &id, RawStorage_ &target) const {
        return target.copy_from(id, *this);
    }

    tl::expected<void, copy_error> copy_from(const Id &id, const RawStorage_ &source) {
        const auto context = [&] {
            return fmt::format("copying node {}", id);
        };
        const auto source_path = source.path_for(id);
        const auto target_path = this->path_for(id);
        // Checked first so that a missing source leaves the target untouched.
        if (!source.has(id)) {
            return alp::make_unexpected(io::FileError(std::make_error_code(std::errc::no_such_file_or_directory), source_path), context());
        }

        if (source_path.extension() != target_path.extension()) {
            // TODO: should this error instead?
            ALP_TRY_CONTEXT(const T node, Codec::load_from_path(source_path), context());
            ALP_TRY_CONTEXT(Codec::save_to_path(node, target_path), context());
            return {};
        }

        // The return value only says whether there was a file to remove, so a failure shows in the code alone.
        std::error_code code;
        std::filesystem::remove(target_path, code);
        if (code) {
            return tl::make_unexpected(alp::make_error(io::FileError(code, target_path), fmt::format("removing a node file at {}", target_path.string())).context(context()));
        }

        ALP_TRY_CONTEXT(io::utils::create_parent_directories(target_path), context());

        std::filesystem::create_hard_link(source_path, target_path, code);
        if (code) {
            return tl::make_unexpected(alp::make_error(io::FileError(code, target_path), fmt::format("linking a node file from {} to {}", source_path.string(), target_path.string())).context(context()));
        }

        return {};
    }

    bool remove(const Id &id) const noexcept {
        const auto path = this->path_for(id);
        std::error_code ec;
        const auto removed = std::filesystem::remove(path, ec);
        return !ec && removed;
    }

    bool has(const Id &id) const noexcept {
        const auto path = this->path_for(id);
        std::error_code ec;
        const auto exists = std::filesystem::exists(path, ec);
        return !ec && exists;
    }

    std::filesystem::path path_for(const Id &id) const noexcept {
        return this->_layout.get_node_path(id);
    }

    std::filesystem::path base_path() const noexcept {
        return this->_layout.base_path();
    }

    const disk::Layout &layout() const noexcept {
        return this->_layout;
    }

private:
    disk::Layout _layout;
};

} // namespace octree
