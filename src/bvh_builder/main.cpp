#include <cstdlib>
#include <exception>
#include <utility>

#include <fmt/format.h>
#include <libassert/assert.hpp>
#include <tl/expected.hpp>

#include "frontier/build.h"
#include "cli.h"
#include "build/lbvh.h"
#include "build/ploc.h"
#include "bvh/flatten_dag.h"
#include "error/Result.h"
#include "error/format.h"
#include "error/log.h"
#include "error/try.h"
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
[[nodiscard]]
alp::Result<bvh::Reference, io::Error> build_tree(
    const cli::Args &args, frontier::Storage sorted, bvh::Storage &nodes) {
    switch (args.mode) {
    case cli::Mode::Lbvh:
        return lbvh::build(sorted, nodes, lbvh::Options{
            .max_batch_bytes = args.max_batch_bytes,
        });
    case cli::Mode::Ploc:
        return ploc::build(std::move(sorted), nodes, args.scratch_path / "rounds", ploc::Options{
            .max_batch_bytes = args.max_batch_bytes,
        });
    }
    UNREACHABLE("unknown build mode");
}

// Builds the bvh over the dag at the input path, writing it to the output path.
[[nodiscard]]
alp::Result<void> build_bvh(const cli::Args &args) {
    const octree::IndexedDagMetaStorage dag = octree::open_folder_indexed<dag::NodeMetadata>(args.input_path);

    // The binary tree is what widening consumes, so only its widened form is the run being asked for.
    bvh::Storage nodes(args.scratch_path / "binary");

    const bvh::FlattenOptions flatten_options{
        .max_batch_bytes = args.max_batch_bytes,
    };
    ALP_TRY(bvh::flatten_dag(dag, nodes, flatten_options));
    ALP_TRY(nodes.save_index());
    LOG_INFO("Flattened {} into {} node batches", args.input_path.string(), nodes.size());

    frontier::Storage run(args.scratch_path / "leaves");

    const frontier::BuildOptions frontier_options{
        .max_batch_bytes = args.max_batch_bytes,
        .scene_bounds = bvh::earth_bounds(),
    };
    ALP_TRY(frontier::build_from_leaves(nodes, run, frontier_options));
    ALP_TRY(run.save_index());
    LOG_INFO("Built a frontier of {} batches", run.size());

    frontier::Storage sorted(args.scratch_path / "sorted");

    const frontier::SortOptions sort_options{
        .max_batch_bytes = args.max_batch_bytes,
    };
    ALP_TRY(frontier::sort_by_morton(run, sorted, sort_options));
    ALP_TRY(sorted.save_index());
    LOG_INFO("Sorted the frontier into {} batches", sorted.size());

    ALP_TRY(const bvh::Reference root, build_tree(args, std::move(sorted), nodes));
    LOG_INFO("Built the tree, with its root in batch {} at node {}", root.batch, root.node);

    bvh::Storage wide(args.output_path);

    const widen::Options widen_options{
        .max_batch_bytes = args.max_batch_bytes,
        .max_resident_bytes = 4 * args.max_batch_bytes,
    };
    ALP_TRY(const bvh::Reference wide_root, widen::build(nodes, root, wide, args.scratch_path, widen_options));
    LOG_INFO("Widened the tree into {} batches, with its root in batch {} at node {}", wide.size(), wide_root.batch, wide_root.node);
    return {};
}

}

int main(int argc, char **argv) {
    const cli::Args args = cli::parse(argc, argv);

    Log::init(args.log_level);

    try {
        const alp::Result<void> result = build_bvh(args);
        if (!result.has_value()) {
            LOG_UNEXPECTED(result.error());
            return EXIT_FAILURE;
        }
        return EXIT_SUCCESS;
    } catch (const std::exception &e) {
        LOG_ERROR("{}", e.what());
        return EXIT_FAILURE;
    }
}
