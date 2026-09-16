#pragma once

#include <cstddef>

// Default maximum batch size in memory.
constexpr size_t default_max_batch_bytes = 100 * 1024 * 1024;

// Default maximum size of everything kept resident in memory at once.
constexpr size_t default_max_resident_bytes = size_t{4} * 1024 * 1024 * 1024;
