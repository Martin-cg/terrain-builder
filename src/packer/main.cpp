#include <cstdlib>

#include "cli.h"
#include "log.h"

int main(int argc, char **argv) {
    const cli::Args args = cli::parse(argc, argv);

    Log::init(args.log_level);

    LOG_ERROR("Packing {} into {} is not implemented yet", args.input_path.string(), args.output_path.string());
    return EXIT_FAILURE;
}
