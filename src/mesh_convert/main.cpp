#include <cstdlib>
#include <expected>
#include <filesystem>

#include <glm/glm.hpp>

#include "error/Result.h"
#include "error/log.h"
#include "error/try.h"
#include "mesh/SimpleMesh.h"
#include "mesh/io.h"
#include "opencv_utils.h"
#include "log.h"
#include "cli.h"

[[nodiscard]]
alp::Result<void> run(const cli::Args& args) {
    LOG_INFO("Loading input mesh...");
    ALP_TRY(SimpleMesh mesh, mesh::io::load_from_path(args.input_path));

    if (args.texture_resolution.has_value()) {
        if (mesh.texture.has_value()) {
            cv::Mat& texture = mesh.texture.value();
            glm::uvec2 target_resolution = args.texture_resolution.value();
            LOG_INFO("Resizing mesh texture to {}x{}", target_resolution.x, target_resolution.y);
            rescale_texture_inplace(texture, target_resolution);
        } else {
            LOG_WARN("--texture-resolution specified but no texture present");
        }
    }

    LOG_INFO("Writing output mesh...");
    ALP_TRY(mesh::io::save_to_path(mesh, args.output_path));
    return {};
}

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
