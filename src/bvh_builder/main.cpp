#include <cstdlib>
#include <exception>

#include "frontier/build.h"
#include "cli.h"
#include "bvh/flatten_dag.h"
#include "log.h"
#include "octree/OddLevelShifted.h"
#include "octree/storage/open.h"
#include "../dag_builder/storage.h"
#include "bvh/Storage.h"
#include "bvh/earth.h"
#include "frontier/Storage.h"

int main(int argc, char **argv) {
    const cli::Args args = cli::parse(argc, argv);

    Log::init(args.log_level);

    try {
        const octree::IndexedDagMetaStorage dag = octree::open_folder_indexed<dag::NodeMetadata>(args.input_path);

        std::filesystem::create_directories(args.output_path / "nodes");
        bvh::Storage nodes(args.output_path / "nodes");

        const auto flattened = bvh::flatten_dag(dag, nodes, bvh::FlattenOptions{
            .max_batch_bytes = args.max_batch_bytes,
        });
        if (!flattened) {
            LOG_ERROR("Failed to flatten {}: {}", args.input_path.string(), flattened.error());
            return EXIT_FAILURE;
        }

        LOG_INFO("Flattened {} into {} node batches", args.input_path.string(), nodes.size());
        if (!nodes.save_index()) {
            return EXIT_FAILURE;
        }

        std::filesystem::create_directories(args.output_path / "frontier");
        frontier::Storage run(args.output_path / "frontier");

        const auto built = frontier::build_from_leaves(nodes, run, frontier::BuildOptions{
            .max_batch_bytes = args.max_batch_bytes,
            .scene_bounds = bvh::earth_bounds()
        });
        if (!built) {
            LOG_ERROR("Failed to build the frontier: {}", built.error());
            return EXIT_FAILURE;
        }

        LOG_INFO("Built a frontier of {} batches", run.size());
        return run.save_index() ? EXIT_SUCCESS : EXIT_FAILURE;
    } catch (const std::exception &e) {
        LOG_ERROR("{}", e.what());
        return EXIT_FAILURE;
    }
}
