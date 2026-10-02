#include <cstdlib>
#include <filesystem>

#include "cli.h"
#include "dag_node.h"
#include "encoded.h"
#include "error/Result.h"
#include "error/log.h"
#include "error/try.h"
#include "log.h"
#include "mesh/io.h"
#include "octree/storage/MeshStorage.h"
#include "octree/storage/codec/ZppBitsCodec.h"
#include "octree/storage/open.h"
#include "ProgressIndicator.h"
#include "storage.h"
#include "utils.h"

namespace {

[[nodiscard]]
alp::Result<void> export_node(const cli::Args &args) {
    ALP_TRY(const dag::ClusterBatch &node, octree::ZppBitsCodec<dag::ClusterBatch>::load_from_path(args.input_path));

    const mesh::Simple mesh = clustering_to_mesh(node.clustering);

    ALP_TRY(mesh::io::save_to_path(mesh, args.output_path));
    return {};
}

[[nodiscard]]
alp::Result<void> export_storage(const cli::Args &args) {
    ALP_TRY(const octree::IndexedDagStorage input_storage, octree::open_folder_indexed<dag::ClusterBatch>(args.input_path));

    ALP_TRY(octree::MeshStorage output_storage, (octree::open_folder<mesh::Simple, octree::MeshCodec>(
        args.output_path,
        false,
        octree::OpenOptions{.preferred_extension_with_dot = ".glb"})));
    output_storage.settings().allow_overwrite = true;

    size_t exported_count = 0;

    ProgressIndicator progress(input_storage.index().size());
    auto progress_thread = progress.start_monitoring();

    for (const auto &[id, status] : input_storage.index()) {
        if (status == octree::NodeStatus::Virtual) {
            progress.task_finished();
            continue;
        }

        const auto load_result = input_storage.load(id);
        if (!load_result.has_value()) {
            LOG_UNEXPECTED(load_result.error());
            progress.task_finished();
            continue;
        }

        const mesh::Simple mesh = clustering_to_mesh(load_result.value().clustering);

        const auto save_result = output_storage.save(id, mesh);
        if (!save_result.has_value()) {
            LOG_UNEXPECTED(save_result.error());
            progress.task_finished();
            continue;
        }

        exported_count++;
        progress.task_finished();
    }

    progress_thread.join();

    ALP_TRY(output_storage.save_or_create_index());

    LOG_INFO("Exported {} debug meshes to {}", exported_count, args.output_path);
    return {};
}

[[nodiscard]]
alp::Result<void> run(const cli::Args &args) {
    if (std::filesystem::is_directory(args.input_path)) {
        return export_storage(args);
    } else {
        return export_node(args);
    }
}

} // namespace

int main(int argc, char **argv) {
    const cli::Args args = cli::parse(argc, argv);
    Log::init(args.log_level);

    const alp::Result<void> result = run(args);
    if (!result.has_value()) {
        LOG_UNEXPECTED(result.error());
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
