#include <cstdlib>
#include <exception>

#include "cli.h"
#include "flatten_dag.h"
#include "log.h"
#include "octree/storage/open.h"

#include "../dag_builder/storage.h"
#include "storage.h"

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
        return nodes.save() ? EXIT_SUCCESS : EXIT_FAILURE;
    } catch (const std::exception &e) {
        LOG_ERROR("{}", e.what());
        return EXIT_FAILURE;
    }
}
