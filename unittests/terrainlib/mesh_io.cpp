/*****************************************************************************
 * Alpine Terrain Builder
 * Copyright (C) 2022 Adam Celarek <last name at cg tuwien ac at>
 * Copyright (C) 2022 alpinemaps.org
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *****************************************************************************/

#include <filesystem>
#include <fstream>
#include <system_error>

#include <opencv2/opencv.hpp>
#include <fmt/format.h>

#include "../catch2_helpers.h"
#include "../opencv_helpers.h"
#include "../temp_directory.h"
#include "error/format.h"
#include "io/Error.h"
#include "mesh/io.h"
#include "mesh/io/error.h"
#include "mesh/encode.h"
#include "octree/storage/codec/MeshCodec.h"

TEST_CASE("transcode roundtrip") {
    mesh::Simple mesh;

    mesh.positions.push_back(glm::dvec3(0, 0, 0));
    mesh.positions.push_back(glm::dvec3(1, 0, 0));
    mesh.positions.push_back(glm::dvec3(0, 1, 0));
    mesh.positions.push_back(glm::dvec3(1, 1, 0));

    mesh.triangles.push_back(glm::uvec3(0, 2, 1));
    mesh.triangles.push_back(glm::uvec3(1, 2, 3));

    mesh.uvs.push_back(glm::dvec2(0, 0));
    mesh.uvs.push_back(glm::dvec2(1, 0));
    mesh.uvs.push_back(glm::dvec2(0, 1));
    mesh.uvs.push_back(glm::dvec2(1, 1));

    mesh.texture = cv::Mat3b(100, 100);
    cv::randu(*mesh.texture, cv::Scalar(0, 0, 0), cv::Scalar(256, 256, 256));

    const tl::expected<mesh::Encoded, mesh::EncodeError> encode_result =
        mesh::encode(mesh, mesh::EncodeOptions{.texture_format = ".png"});
    if (!encode_result.has_value()) {
        FAIL(fmt::to_string(encode_result.error()));
    }
    const mesh::Encoded encoded = encode_result.value();

    const tl::expected<mesh::Simple, mesh::DecodeError> decode_result =
        mesh::decode(encoded, mesh::DecodeOptions{});
    if (!decode_result.has_value()) {
        FAIL(fmt::to_string(decode_result.error()));
    }

    const SimpleMesh roundtrip_mesh = decode_result.value();
    CHECK(roundtrip_mesh.positions == mesh.positions);
    CHECK(roundtrip_mesh.uvs == mesh.uvs);
    CHECK(roundtrip_mesh.triangles == mesh.triangles);
    CHECK(roundtrip_mesh.texture.has_value());
    CHECK(mat_equals(*roundtrip_mesh.texture, *mesh.texture));
}

TEST_CASE("io roundtrip") {
    for (const auto& format : {"glb", "gltf"}) {
        DYNAMIC_SECTION(format) {
            SimpleMesh mesh;

            mesh.positions.push_back(glm::dvec3(0, 0, 0));
            mesh.positions.push_back(glm::dvec3(1, 0, 0));
            mesh.positions.push_back(glm::dvec3(0, 1, 0));
            mesh.positions.push_back(glm::dvec3(1, 1, 0));

            mesh.triangles.push_back(glm::uvec3(0, 2, 1));
            mesh.triangles.push_back(glm::uvec3(1, 2, 3));

            mesh.uvs.push_back(glm::dvec2(0, 0));
            mesh.uvs.push_back(glm::dvec2(1, 0));
            mesh.uvs.push_back(glm::dvec2(0, 1));
            mesh.uvs.push_back(glm::dvec2(1, 1));

            mesh.texture = cv::Mat3b(100, 100);
            cv::randu(*mesh.texture, cv::Scalar(0, 0, 0), cv::Scalar(256, 256, 256));

            const std::filesystem::path mesh_path = fmt::format("./unittests/output/mesh.{}", format);
            std::filesystem::remove(mesh_path);
            CHECK(!std::filesystem::exists(mesh_path));

            REQUIRE(mesh::io::save_to_path(mesh, mesh_path, mesh::io::SaveOptions{.texture_format = ".png"}));
            CHECK(std::filesystem::exists(mesh_path));

            const mesh::io::Result<SimpleMesh> result = mesh::io::load_from_path(mesh_path);
            if (!result.has_value()) {
                FAIL(fmt::to_string(result.error()));
            }
            std::filesystem::remove(mesh_path);
            const SimpleMesh roundtrip_mesh = result.value();
            CHECK(roundtrip_mesh.positions == mesh.positions);
            CHECK(roundtrip_mesh.uvs == mesh.uvs);
            CHECK(roundtrip_mesh.triangles == mesh.triangles);
            CHECK(roundtrip_mesh.texture.has_value());
            CHECK(mat_equals(*roundtrip_mesh.texture, *mesh.texture));
        }
    }
}

TEST_CASE("io roundtrip high precision") {
    for (const auto& format : {"terrain"}) {
        DYNAMIC_SECTION(format) {
            SimpleMesh mesh;

            const double pi = std::numbers::pi_v<double>;
            CHECK((double)(float)pi != pi);

            mesh.positions.push_back(glm::dvec3(0, 0, 0));
            mesh.positions.push_back(glm::dvec3(pi, 0, 0));
            mesh.positions.push_back(glm::dvec3(0, pi, 0));

            mesh.triangles.push_back(glm::uvec3(0, 2, 1));

            mesh.uvs.push_back(glm::dvec2(0, 0));
            mesh.uvs.push_back(glm::dvec2(1, 0));
            mesh.uvs.push_back(glm::dvec2(0, 1));

            mesh.texture = cv::Mat3b(100, 100);
            cv::randu(*mesh.texture, cv::Scalar(0, 0, 0), cv::Scalar(256, 256, 256));

            const std::filesystem::path mesh_path = fmt::format("./unittests/output/mesh.{}", format);
            std::filesystem::remove(mesh_path);
            CHECK(!std::filesystem::exists(mesh_path));

            REQUIRE(mesh::io::save_to_path(mesh, mesh_path, mesh::io::SaveOptions{.texture_format = ".png"}));
            CHECK(std::filesystem::exists(mesh_path));

            const mesh::io::Result<SimpleMesh> result = mesh::io::load_from_path(mesh_path);
            if (!result.has_value()) {
                FAIL(fmt::to_string(result.error()));
            }
            std::filesystem::remove(mesh_path);
            const SimpleMesh roundtrip_mesh = result.value();
            CHECK(roundtrip_mesh.positions == mesh.positions);
            CHECK(roundtrip_mesh.uvs == mesh.uvs);
            CHECK(roundtrip_mesh.triangles == mesh.triangles);
            CHECK(roundtrip_mesh.texture.has_value());
            CHECK(mat_equals(*roundtrip_mesh.texture, *mesh.texture));
        }
    }
}

TEST_CASE("io roundtrip no texture") {
    for (const auto &format : {"gltf", "glb", "terrain"}) {
        DYNAMIC_SECTION(format) {
            SimpleMesh mesh;

            const double pi = std::numbers::pi_v<double>;
            CHECK((double)(float)pi != pi);

            mesh.positions.push_back(glm::dvec3(0, 0, 0));
            mesh.positions.push_back(glm::dvec3(1, 0, 0));
            mesh.positions.push_back(glm::dvec3(0, 1, 0));
            mesh.positions.push_back(glm::dvec3(1, 1, 0));

            mesh.triangles.push_back(glm::uvec3(0, 2, 1));
            mesh.triangles.push_back(glm::uvec3(1, 2, 3));

            mesh.uvs.push_back(glm::dvec2(0, 0));
            mesh.uvs.push_back(glm::dvec2(1, 0));
            mesh.uvs.push_back(glm::dvec2(0, 1));
            mesh.uvs.push_back(glm::dvec2(1, 1));

            const std::filesystem::path mesh_path = fmt::format("./unittests/output/mesh.{}", format);
            std::filesystem::remove(mesh_path);
            CHECK(!std::filesystem::exists(mesh_path));

            REQUIRE(mesh::io::save_to_path(mesh, mesh_path));
            CHECK(std::filesystem::exists(mesh_path));

            const mesh::io::Result<SimpleMesh> result = mesh::io::load_from_path(mesh_path);
            if (!result.has_value()) {
                FAIL(fmt::to_string(result.error()));
            }
            // std::filesystem::remove(mesh_path);
            const SimpleMesh roundtrip_mesh = result.value();
            CHECK(roundtrip_mesh.positions == mesh.positions);
            CHECK(roundtrip_mesh.uvs == mesh.uvs);
            CHECK(roundtrip_mesh.triangles == mesh.triangles);
            CHECK(!roundtrip_mesh.texture.has_value());
        }
    }
}

TEST_CASE("io roundtrip no texture and uvs") {
    for (const auto &format : {"gltf", "glb", "terrain"}) {
        DYNAMIC_SECTION(format) {
            SimpleMesh mesh;

            const double pi = std::numbers::pi_v<double>;
            CHECK((double)(float)pi != pi);

            mesh.positions.push_back(glm::dvec3(0, 0, 0));
            mesh.positions.push_back(glm::dvec3(1, 0, 0));
            mesh.positions.push_back(glm::dvec3(0, 1, 0));
            mesh.positions.push_back(glm::dvec3(1, 1, 0));

            mesh.triangles.push_back(glm::uvec3(0, 2, 1));
            mesh.triangles.push_back(glm::uvec3(1, 2, 3));

            const std::filesystem::path mesh_path = fmt::format("./unittests/output/mesh.{}", format);
            std::filesystem::remove(mesh_path);
            CHECK(!std::filesystem::exists(mesh_path));

            REQUIRE(mesh::io::save_to_path(mesh, mesh_path));
            CHECK(std::filesystem::exists(mesh_path));

            const mesh::io::Result<SimpleMesh> result = mesh::io::load_from_path(mesh_path);
            if (!result.has_value()) {
                FAIL(fmt::to_string(result.error()));
            }
            std::filesystem::remove(mesh_path);
            const SimpleMesh roundtrip_mesh = result.value();
            CHECK(roundtrip_mesh.positions == mesh.positions);
            CHECK(!roundtrip_mesh.has_uvs());
            CHECK(roundtrip_mesh.triangles == mesh.triangles);
            CHECK(!roundtrip_mesh.texture.has_value());
        }
    }
}

namespace {

// A mesh of a single triangle.
SimpleMesh make_triangle() {
    SimpleMesh mesh;
    mesh.positions.push_back(glm::dvec3(0, 0, 0));
    mesh.positions.push_back(glm::dvec3(1, 0, 0));
    mesh.positions.push_back(glm::dvec3(0, 1, 0));
    mesh.triangles.push_back(glm::uvec3(0, 1, 2));
    return mesh;
}

} // namespace

TEST_CASE("A glTF error formats what cgltf's result means") {
    CHECK(fmt::to_string(mesh::io::GltfError(cgltf_result_file_not_found)) == "the file was not found");
}

TEST_CASE("Loading a mesh with an unsupported extension fails, naming the extension") {
    const mesh::io::Result<SimpleMesh> result = mesh::io::load_from_path("mesh.xyz");

    REQUIRE(!result.has_value());
    CHECK(result.error().narrow<mesh::io::UnsupportedFormat>().has_value());
    CHECK(fmt::to_string(result.error()).contains(".xyz"));
}

TEST_CASE("Saving a mesh with an unsupported extension fails, naming the extension") {
    const TempDirectory directory("mesh_io_unsupported_save");
    const mesh::io::Result<void> result = mesh::io::save_to_path(make_triangle(), directory.path() / "mesh.xyz");

    REQUIRE(!result.has_value());
    CHECK(result.error().narrow<mesh::io::UnsupportedFormat>().has_value());
    CHECK(fmt::to_string(result.error()).contains(".xyz"));
}

TEST_CASE("Loading a missing terrain file fails with a file error") {
    const TempDirectory directory("mesh_io_missing_terrain");
    const mesh::io::Result<SimpleMesh> result = mesh::io::load_from_path(directory.path() / "missing.terrain");

    REQUIRE(!result.has_value());
    const io::FileError *file_error = result.error().get_if<io::FileError>();
    REQUIRE(file_error != nullptr);
    CHECK(*file_error == std::errc::no_such_file_or_directory);
}

TEST_CASE("Loading a missing glTF file fails with a glTF error") {
    const TempDirectory directory("mesh_io_missing_gltf");
    const mesh::io::Result<SimpleMesh> result = mesh::io::load_from_path(directory.path() / "missing.gltf");

    REQUIRE(!result.has_value());
    CHECK(result.error().holds(mesh::io::GltfError(cgltf_result_file_not_found)));
}

TEST_CASE("Loading a terrain file with truncated data fails with a serde error") {
    const TempDirectory directory("mesh_io_truncated_terrain");
    const std::filesystem::path path = directory.path() / "mesh.terrain";
    REQUIRE(mesh::io::save_to_path(make_triangle(), path).has_value());
    std::filesystem::resize_file(path, std::filesystem::file_size(path) - 1);

    const mesh::io::Result<SimpleMesh> result = mesh::io::load_from_path(path);

    REQUIRE(!result.has_value());
    CHECK(result.error().narrow<io::SerdeError>().has_value());
}

TEST_CASE("Loading a glTF file without a mesh fails as unsupported") {
    const TempDirectory directory("mesh_io_incomplete_gltf");
    const std::filesystem::path path = directory.path() / "empty.gltf";
    std::ofstream(path) << R"({"asset":{"version":"2.0"}})";

    const mesh::io::Result<SimpleMesh> result = mesh::io::load_from_path(path);

    REQUIRE(!result.has_value());
    CHECK(result.error().narrow<mesh::io::UnsupportedGltf>().has_value());
}

TEST_CASE("The mesh codec reports a missing file as a file error, naming the path") {
    const mesh::io::Error error = octree::MeshCodec::file_not_found("some/node.terrain");

    const io::FileError *file_error = error.get_if<io::FileError>();
    REQUIRE(file_error != nullptr);
    CHECK(*file_error == std::errc::no_such_file_or_directory);
    CHECK(fmt::to_string(error).contains("some/node.terrain"));
}
