#include <filesystem>
#include <optional>
#include <vector>

#define CATCH_CONFIG_ENABLE_BENCHMARKING
#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/catch_test_macros.hpp>

#include "mesh/SimpleMesh.h"
#include "mesh/bounds.h"
#include "mesh/clip.h"
#include "mesh/io.h"
#include "octree/Id.h"
#include "octree/Space.h"

TEST_CASE("mesh::clip_on_bounds benchmark", "[mesh][clip][!benchmark]") {
    BENCHMARK_ADVANCED("clip based on octree")(Catch::Benchmark::Chronometer meter) {
        const std::filesystem::path mesh_path = ALP_TEST_DATA_DIR "/meshes/stephansdom_16_51832_38369_54849.terrain";
        auto mesh_result = mesh::io::load_from_path(mesh_path);
        REQUIRE(mesh_result.has_value());
        SimpleMesh &mesh = mesh_result.value();
        mesh.uvs.clear();
        mesh.texture = std::nullopt;

        const octree::Space space = octree::Space::earth();
        const auto mesh_bounds = mesh::calculate_bounds(mesh);
        const octree::Id id = space.find_smallest_node_encompassing_bounds(mesh_bounds).value();
        const auto child_ids = id.children().value();
        std::vector<octree::Bounds> child_bounds;
        child_bounds.reserve(child_ids.size());
        for (const auto &child_id : child_ids) {
            const auto bounds = space.get_node_bounds(child_id);
            child_bounds.push_back(bounds);
        }

        meter.measure([mesh, child_bounds] {
            std::vector<SimpleMesh> child_meshes;
            child_meshes.reserve(child_bounds.size());
            for (const auto &bounds : child_bounds) {
                const auto clipped_mesh = mesh::clip_on_bounds(mesh, bounds).into_owned();
                child_meshes.push_back(clipped_mesh);
            }
            return child_meshes;
        });
    };
}
