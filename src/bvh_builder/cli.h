#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>

#include <spdlog/spdlog.h>

namespace cli {

// Which algorithm builds the tree over the frontier.
enum class Mode : uint8_t {
    Lbvh,
    GatedPloc,
    Hploc,
};

struct Args {
    spdlog::level::level_enum log_level;

    Mode mode;
    std::filesystem::path input_path;
    std::filesystem::path output_path;
    std::filesystem::path scratch_path;
    size_t max_batch_bytes;
    size_t max_resident_bytes;
};

Args parse(int argc, const char *const *argv);

}
