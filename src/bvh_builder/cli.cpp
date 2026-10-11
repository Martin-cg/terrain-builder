
#include <CLI/CLI.hpp>
#include <libassert/assert.hpp>
#include <spdlog/spdlog.h>

#include <cstdlib>
#include <string>
#include <unordered_map>

#include "cli.h"
#include "bvh/Storage.h"
#include "storage/batch_limits.h"

using namespace cli;

namespace {

const std::unordered_map<std::string, Mode> MODE_NAMES{
    {"pseudo-lbvh", Mode::PseudoLbvh},
    {"lbvh", Mode::Lbvh},
    {"gated-ploc", Mode::GatedPloc},
    {"hploc", Mode::Hploc},
};

const std::unordered_map<std::string, spdlog::level::level_enum> LOG_LEVEL_NAMES{
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
        ->transform(CLI::CheckedTransformer(MODE_NAMES, CLI::ignore_case))
        ->default_val("gated-ploc");

    app.add_option("--max-batch-size", args.max_batch_bytes, "Maximum size a batch file, e.g. 512MB or 1GB")
        ->transform(CLI::AsSizeValue(false))
        ->default_val(DEFAULT_MAX_BATCH_BYTES)
        ->check(CLI::PositiveNumber);

    app.add_option("--max-resident-size", args.max_resident_bytes, "Target maximum memory to keep resident (e.g. for caches)")
        ->transform(CLI::AsSizeValue(false))
        ->default_val(DEFAULT_MAX_RESIDENT_BYTES)
        ->check(CLI::PositiveNumber);

    app.add_option("--verbosity", args.log_level, "Verbosity level of logging")
        ->transform(CLI::CheckedTransformer(LOG_LEVEL_NAMES, CLI::ignore_case))
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
