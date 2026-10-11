#pragma once

#include <cstdint>
#include <filesystem>
#include <utility>

#include <fmt/format.h>

#include "error/Result.h"
#include "io/DirectoryNotEmpty.h"
#include "io/FileError.h"
#include "io/ScratchDirectory.h"

#include "frontier/Storage.h"

namespace gated_ploc {

namespace detail {

class RoundLayout {
public:
    explicit RoundLayout(std::filesystem::path root) : _root(std::move(root)) {}

    // The run a round writes, in whatever order its chunks produced it.
    [[nodiscard]]
    alp::Result<frontier::Storage, io::FileError, io::DirectoryNotEmpty> create_merge_scratch(const uint32_t round) const {
        return frontier::Storage::create(this->get_round_path(round) / "merged");
    }

    // The same run in morton order, which is what the next round reads.
    [[nodiscard]]
    alp::Result<frontier::Storage, io::FileError, io::DirectoryNotEmpty> create_sort_scratch(const uint32_t round) const {
        return frontier::Storage::create(this->get_round_path(round) / "sorted");
    }

    // The directory the sort of a round keeps its runs in, removed with the object returned.
    [[nodiscard]]
    alp::Result<io::ScratchDirectory, io::FileError, io::DirectoryNotEmpty> create_sort_runs_scratch(const uint32_t round) const {
        return io::create_scratch_directory(this->get_round_path(round) / "sort-runs");
    }

private:
    [[nodiscard]]
    std::filesystem::path get_round_path(const uint32_t round) const {
        return this->_root / fmt::format("round-{:04}", round);
    }

    std::filesystem::path _root;
};

} // namespace detail

} // namespace gated_ploc
