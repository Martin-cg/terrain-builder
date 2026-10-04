#pragma once

#include "../result_helpers.h"

#include <cstdint>
#include <filesystem>
#include <vector>

#include "range_utils.h"

#include "frontier/Storage.h"
#include "frontier/layout.h"

// A saved storage in `directory` holding one batch per element of `batches`.
inline frontier::Storage make_storage(const std::filesystem::path &directory, const std::vector<std::vector<frontier::Entry>> &batches) {
    frontier::Storage storage = REQUIRE_VALUE(frontier::Storage::create(directory));
    for (const std::vector<frontier::Entry> &entries : batches) {
        REQUIRE_VALUE(storage.write(frontier::Batch{.entries = entries}));
    }
    REQUIRE_VALUE(storage.save_index());
    return storage;
}

// The entries of every batch of `storage`, in order.
inline std::vector<std::vector<frontier::Entry>> read_batches(const frontier::Storage &storage) {
    std::vector<std::vector<frontier::Entry>> result;
    for (const uint32_t index : range(storage.size())) {
        const frontier::Batch batch = REQUIRE_VALUE(storage.read(index));
        result.push_back(batch.entries);
    }
    return result;
}
