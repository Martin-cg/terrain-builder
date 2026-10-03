#include <algorithm>
#include <filesystem>
#include <memory>
#include <optional>
#include <system_error>
#include <unordered_set>
#include <vector>

#include <fmt/format.h>

#include "error/Context.h"
#include "error/log.h"
#include "error/try.h"
#include "error/utils.h"
#include "io/FileError.h"
#include "io/filesystem.h"
#include "io/serialize.h"
#include "log.h"
#include "io/Error.h"
#include "octree/disk/IndexFile.h"
#include "octree/disk/layout/StrategyRegister.h"
#include "octree/storage/IndexedStorage.h"
#include "octree/storage/helpers.h"
#include "octree/storage/defaults.h"
#include "range_utils.h"

namespace octree {

template <typename T, CodecFor<T> Codec>
alp::Result<IndexedStorage_<T, Codec>, OpenError> open_index(const std::filesystem::path &index_path) {
    LOG_TRACE("Opening storage index {}", index_path);
    const auto context = alp::make_context("opening an index from {}", index_path);

    ALP_TRY_CONTEXT(
        disk::v1::IndexFile index_file,
        io::read_from_path<disk::v1::IndexFile>(index_path),
        context);
    LOG_TRACE("Successfully read storage index with {} entries.", index_file.map.size());
    auto index_map = std::move(index_file.map);
    const auto base_path = index_path.parent_path();
    auto layout_strategy = disk::layout::StrategyRegister::instance().create(index_file.layout_strategy_id);
    if (!layout_strategy) {
        return alp::make_unexpected(disk::layout::UnknownLayoutStrategy{.id = index_file.layout_strategy_id}, context);
    }
    disk::Layout layout(base_path, std::move(layout_strategy), index_file.preferred_extension);
    RawStorage_<T, Codec> raw_storage(std::move(layout));
    return IndexedStorage_<T, Codec>(std::move(raw_storage), std::move(index_map));
}

template <typename T, CodecFor<T> Codec>
alp::Result<Storage_<T, Codec>, OpenError> open_folder(
    const std::filesystem::path &base_path,
    bool create_index,
    OpenOptions options) {
    LOG_TRACE("Opening storage folder {}", base_path);
    const auto context = alp::make_context("opening storage at {}", base_path);

    ALP_TRY_CONTEXT(const bool is_directory, io::filesystem::is_directory(base_path), context);
    if (!is_directory) {
        ALP_TRY_CONTEXT(const bool exists, io::filesystem::exists(base_path), context);
        if (exists) {
            return alp::make_unexpected(io::FileError(std::make_error_code(std::errc::not_a_directory), base_path), context);
        }

        LOG_TRACE("Base path {} does not exist, creating it", base_path);
        ALP_TRY_CONTEXT(io::filesystem::create_directories(base_path), context);
    }

    const std::filesystem::path index_path = base_path / disk::v1::index_file_name();
    auto storage_result = open_index<T, Codec>(index_path);
    if (storage_result.has_value()) {
        LOG_TRACE("Loaded existing index");
        return Storage_<T, Codec>(std::move(storage_result.value()));
    }

    ALP_TRY_CONTEXT(std::optional<helpers::LayoutWithoutBase> layout_info_opt, helpers::guess_layout_strategy(base_path), context);
    if (layout_info_opt.has_value()) {
        const octree::helpers::LayoutWithoutBase &layout_info = layout_info_opt.value();
        LOG_TRACE("Guessed layout strategy of dataset as {} and extension as {}",
                  disk::layout::StrategyRegister::instance().get_id(*layout_info.strategy),
                  layout_info.extension_with_dot);
    } else {
        auto default_layout_strategy = std::move(options.default_layout_strategy);
        if (!default_layout_strategy) {
            default_layout_strategy = disk::layout::strategy::make_default();
        }
        std::string default_extension_with_dot;
        const std::vector<std::string_view> supported_extensions = Codec::supported_extensions();
        if (options.preferred_extension_with_dot.has_value()) {
            ASSERT(contains(supported_extensions, options.preferred_extension_with_dot.value()));
            default_extension_with_dot = options.preferred_extension_with_dot.value();
        } else {
            default_extension_with_dot = supported_extensions[0];
        }
        LOG_WARN("Unable to determine layout of dataset, using layout strategy {} and extension {}",
                 disk::layout::StrategyRegister::instance().get_id(*default_layout_strategy),
                 default_extension_with_dot);
        layout_info_opt = octree::helpers::LayoutWithoutBase(std::move(default_layout_strategy), default_extension_with_dot);
    }

    disk::Layout layout(base_path, std::move(layout_info_opt->strategy), layout_info_opt->extension_with_dot);
    if (!create_index) {
        return Storage_<T, Codec>(RawStorage_<T, Codec>(std::move(layout)));
    }

    IndexMap map;
    ALP_TRY_CONTEXT(helpers::update_index_map(map, layout), context);
    if (!map.empty()) {
        LOG_UNEXPECTED_OF(helpers::save_index_map(map, layout));
    }
    return Storage_<T, Codec>(RawStorage_<T, Codec>(std::move(layout)), std::move(map));
}

template <typename T, CodecFor<T> Codec>
alp::Result<IndexedStorage_<T, Codec>, OpenError> open_folder_indexed(
    const std::filesystem::path &base_path,
    OpenOptions options) {
    using StorageT = Storage_<T, Codec>;
    ALP_TRY(StorageT storage, (open_folder<T, Codec>(base_path, true, std::move(options))));
    return IndexedStorage_<T, Codec>(std::move(storage));
}

} // namespace octree
