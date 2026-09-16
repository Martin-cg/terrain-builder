#include <cstdlib>
#include <exception>
#include <utility>

#include <libassert/assert.hpp>
#include <tl/expected.hpp>

#include "frontier/build.h"
#include "cli.h"
#include "build/lbvh.h"
#include "build/ploc.h"
#include "bvh/flatten_dag.h"
#include "io/Error.h"
#include "log.h"
#include "octree/OddLevelShifted.h"
#include "octree/storage/open.h"
#include "../dag_builder/storage.h"
#include "bvh/Storage.h"
#include "bvh/earth.h"
#include "frontier/Storage.h"
#include "frontier/sort.h"
#include "widen/build.h"

namespace {

// Builds the tree over the sorted frontier with the algorithm the mode names.
tl::expected<bvh::Reference, io::Error> build_tree(
    const cli::Args &args, frontier::Storage sorted, bvh::Storage &nodes) {
    switch (args.mode) {
    case cli::Mode::Lbvh:
        return lbvh::build(sorted, nodes, lbvh::Options{
            .max_batch_bytes = args.max_batch_bytes,
        });
    case cli::Mode::Ploc:
        return ploc::build(std::move(sorted), nodes, args.scratch_path / "rounds", ploc::Options{
            .max_batch_bytes = args.max_batch_bytes,
            .max_resident_bytes = args.max_resident_bytes,
        });
    }
    UNREACHABLE("unknown build mode");
}

}

int main(int argc, char **argv) {
    const cli::Args args = cli::parse(argc, argv);

    Log::init(args.log_level);

    try {
        const octree::IndexedDagMetaStorage dag = octree::open_folder_indexed<dag::NodeMetadata>(args.input_path);

        // The binary tree is what widening consumes, so only its widened form is the run being asked for.
        bvh::Storage nodes(args.scratch_path / "binary");

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

        frontier::Storage run(args.scratch_path / "leaves");

        const auto built = frontier::build_from_leaves(nodes, run, frontier::BuildOptions{
            .max_batch_bytes = args.max_batch_bytes,
            .scene_bounds = bvh::earth_bounds()
        });
        if (!built) {
            LOG_ERROR("Failed to build the frontier: {}", built.error());
            return EXIT_FAILURE;
        }

        LOG_INFO("Built a frontier of {} batches", run.size());
        if (!run.save_index()) {
            return EXIT_FAILURE;
        }

        frontier::Storage sorted(args.scratch_path / "sorted");

        const auto sorted_result = frontier::sort_by_morton(run, sorted, frontier::SortOptions{
            .max_batch_bytes = args.max_batch_bytes,
            .max_resident_bytes = args.max_resident_bytes,
        });
        if (!sorted_result) {
            LOG_ERROR("Failed to sort the frontier: {}", sorted_result.error());
            return EXIT_FAILURE;
        }
        if (!sorted.save_index()) {
            return EXIT_FAILURE;
        }

        LOG_INFO("Sorted the frontier into {} batches", sorted.size());

        const auto root = build_tree(args, std::move(sorted), nodes);
        if (!root) {
            LOG_ERROR("Failed to build the tree: {}", root.error());
            return EXIT_FAILURE;
        }

        LOG_INFO("Built the tree, with its root in batch {} at node {}", root->batch, root->node);

        bvh::Storage wide(args.output_path);

        const auto wide_root = widen::build(nodes, *root, wide, args.scratch_path, widen::Options{
            .max_batch_bytes = args.max_batch_bytes,
            .max_resident_bytes = args.max_resident_bytes,
        });
        if (!wide_root) {
            LOG_ERROR("Failed to widen the tree: {}", wide_root.error());
            return EXIT_FAILURE;
        }

        LOG_INFO("Widened the tree into {} batches, with its root in batch {} at node {}", wide.size(), wide_root->batch, wide_root->node);
        return EXIT_SUCCESS;
    } catch (const std::exception &e) {
        LOG_ERROR("{}", e.what());
        return EXIT_FAILURE;
    }
}
