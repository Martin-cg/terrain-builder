#pragma once

#include <cstddef>
#include <filesystem>

#include <spdlog/spdlog.h>

namespace cli {

struct Args {
    spdlog::level::level_enum log_level;

    std::filesystem::path input_path;
    std::filesystem::path output_path;
    size_t max_batch_bytes;
};

Args parse(int argc, const char *const *argv);

}
