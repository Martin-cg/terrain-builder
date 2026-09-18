#pragma once

#include <filesystem>

#include <spdlog/spdlog.h>

namespace cli {

struct Args {
    spdlog::level::level_enum log_level;

    std::filesystem::path input_path;
    std::filesystem::path output_path;
};

Args parse(int argc, const char *const *argv);

}
