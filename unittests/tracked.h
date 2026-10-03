#pragma once

#include <cstdint>

// A value that counts how often it was copied and moved.
struct Tracked {
    uint32_t copies = 0;
    uint32_t moves = 0;

    Tracked() = default;
    Tracked(const Tracked &other) : copies(other.copies + 1), moves(other.moves) {}
    Tracked(Tracked &&other) noexcept : copies(other.copies), moves(other.moves + 1) {}
    Tracked &operator=(const Tracked &) = delete;
    Tracked &operator=(Tracked &&) = delete;
};
