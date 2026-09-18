#pragma once

#include <array>
#include <cstdint>

namespace packer {

// The identity of a stored page.
using PageId = std::array<uint8_t, 16>;

}