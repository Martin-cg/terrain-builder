
#include <CLI/CLI.hpp>
#include <libassert/assert.hpp>
#include <spdlog/spdlog.h>

#include <cstdlib>
#include <string>
#include <unordered_map>

#include "cli.h"

using namespace cli;

namespace {

const std::unordered_map<std::string, spdlog::level::level_enum> log_level_names{
    {"off", spdlog::level::level_enum::off},
    {"critical", spdlog::level::level_enum::critical},
    {"error", spdlog::level::level_enum::err},
    {"warn", spdlog::level::level_enum::warn},
    {"info", spdlog::level::level_enum::info},
    {"debug", spdlog::level::level_enum::debug},
    {"trace", spdlog::level::level_enum::trace},
};

}

Args cli::parse(int argc, const char *const *argv) {
    DEBUG_ASSERT(argc >= 0);

    CLI::App app{"packer"};
    app.allow_windows_style_options();

    Args args;

    app.add_option("--input", args.input_path, "Path to the bvh run to pack")
        ->required()
        ->check(CLI::ExistingDirectory);

    app.add_option("--output", args.output_path, "Path to write the pages to")
        ->required();

    app.add_option("--verbosity", args.log_level, "Verbosity level of logging")
        ->transform(CLI::CheckedTransformer(log_level_names, CLI::ignore_case))
        ->default_val("info");

    try {
        app.parse(argc, argv);
    } catch (const CLI::ParseError &e) {
        std::exit(app.exit(e));
    }

    return args;
}
