
#include <CLI/CLI.hpp>
#include <libassert/assert.hpp>
#include <spdlog/spdlog.h>

#include <cstdlib>
#include <string>
#include <unordered_map>

#include "cli.h"
#include "bvh/Storage.h"

using namespace cli;

namespace {

const std::unordered_map<std::string, Mode> mode_names{
    {"lbvh", Mode::Lbvh},
    {"ploc", Mode::Ploc},
};

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

    CLI::App app{"bvh_builder"};
    app.allow_windows_style_options();

    Args args;

    app.add_option("--input", args.input_path, "Path to the dag dataset to flatten")
        ->required()
        ->check(CLI::ExistingDirectory);

    app.add_option("--output", args.output_path, "Path to write the bvh run to")
        ->required();

    app.add_option("--scratch", args.scratch_path, "Path to keep the intermediate runs under")
        ->default_str("<output>-scratch");

    app.add_option("--mode", args.mode, "Which algorithm builds the tree over the frontier")
        ->transform(CLI::CheckedTransformer(mode_names, CLI::ignore_case))
        ->default_val("ploc");

    app.add_option("--max-batch-size", args.max_batch_bytes, "Maximum size a batch file, e.g. 512MB or 1GB")
        ->transform(CLI::AsSizeValue(false))
        ->default_str("64MB")
        ->check(CLI::PositiveNumber);

    app.add_option("--verbosity", args.log_level, "Verbosity level of logging")
        ->transform(CLI::CheckedTransformer(log_level_names, CLI::ignore_case))
        ->default_val("info");

    try {
        app.parse(argc, argv);
    } catch (const CLI::ParseError &e) {
        std::exit(app.exit(e));
    }

    // Kept beside the output, which is the one disk the caller has already chosen for this run.
    if (args.scratch_path.empty()) {
        args.scratch_path = args.output_path;
        // A trailing separator leaves no filename to extend, and would name a directory inside the output.
        if (!args.scratch_path.has_filename()) {
            args.scratch_path = args.scratch_path.parent_path();
        }

        args.scratch_path += "-scratch";
    }

    return args;
}
