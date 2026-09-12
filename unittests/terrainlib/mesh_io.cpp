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

#include <variant>

#include <opencv2/opencv.hpp>
#include <fmt/core.h>

#include "../catch2_helpers.h"
#include "../opencv_helpers.h"
#include "mesh/io.h"
#include "mesh/encode.h"

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
        FAIL(encode_result.error());
    }
    const mesh::Encoded encoded = encode_result.value();

    const tl::expected<mesh::Simple, mesh::DecodeError> decode_result =
        mesh::decode(encoded, mesh::DecodeOptions{});
    if (!decode_result.has_value()) {
        FAIL(decode_result.error());
    }

    const SimpleMesh roundtrip_mesh = decode_result.value();
    CHECK(roundtrip_mesh.positions == mesh.positions);
    CHECK(roundtrip_mesh.uvs == mesh.uvs);
    CHECK(roundtrip_mesh.triangles == mesh.triangles);
    CHECK(roundtrip_mesh.texture.has_value());
    CHECK(mat_equals(*roundtrip_mesh.texture, *mesh.texture));
}

TEST_CASE("mesh::decode rejects a header of another version") {
    mesh::Simple mesh;
    mesh.positions = {glm::dvec3(0, 0, 0), glm::dvec3(1, 0, 0), glm::dvec3(0, 1, 0)};
    mesh.triangles = {glm::uvec3(0, 1, 2)};
    const tl::expected<mesh::Encoded, mesh::EncodeError> encode_result = mesh::encode(mesh);
    REQUIRE(encode_result.has_value());
    mesh::Encoded encoded = *encode_result;
    encoded.header.version = 2;

    const tl::expected<mesh::Simple, mesh::DecodeError> decode_result = mesh::decode(encoded);

    REQUIRE(!decode_result.has_value());
    CHECK(std::holds_alternative<mesh::IncompatibleData>(decode_result.error()));
}

TEST_CASE("mesh::decode reports which stream failed to decode") {
    mesh::Simple mesh;
    mesh.positions = {glm::dvec3(0, 0, 0), glm::dvec3(1, 0, 0), glm::dvec3(0, 1, 0)};
    mesh.triangles = {glm::uvec3(0, 1, 2)};
    const tl::expected<mesh::Encoded, mesh::EncodeError> encode_result = mesh::encode(mesh);
    REQUIRE(encode_result.has_value());
    mesh::Encoded encoded = *encode_result;
    encoded.positions.pop_back();

    const tl::expected<mesh::Simple, mesh::DecodeError> decode_result = mesh::decode(encoded);

    REQUIRE(!decode_result.has_value());
    const mesh::StreamDecodeError *error = std::get_if<mesh::StreamDecodeError>(&decode_result.error());
    REQUIRE(error != nullptr);
    CHECK(error->stream == mesh::MeshStream::Positions);
}

TEST_CASE("mesh::decode reports a texture it cannot decode") {
    mesh::Simple mesh;
    mesh.positions = {glm::dvec3(0, 0, 0), glm::dvec3(1, 0, 0), glm::dvec3(0, 1, 0)};
    mesh.triangles = {glm::uvec3(0, 1, 2)};
    mesh.uvs = {glm::dvec2(0, 0), glm::dvec2(1, 0), glm::dvec2(0, 1)};
    mesh.texture = cv::Mat3b(4, 4, cv::Vec3b(0, 0, 0));
    const tl::expected<mesh::Encoded, mesh::EncodeError> encode_result = mesh::encode(mesh, mesh::EncodeOptions{.texture_format = ".png"});
    REQUIRE(encode_result.has_value());
    mesh::Encoded encoded = *encode_result;
    encoded.texture = {1, 2, 3, 4};

    const tl::expected<mesh::Simple, mesh::DecodeError> decode_result = mesh::decode(encoded);

    REQUIRE(!decode_result.has_value());
    CHECK(std::holds_alternative<mesh::TextureDecodeError>(decode_result.error()));
}

TEST_CASE("mesh::encode reports a texture it cannot encode") {
    mesh::Simple mesh;
    mesh.positions = {glm::dvec3(0, 0, 0), glm::dvec3(1, 0, 0), glm::dvec3(0, 1, 0)};
    mesh.triangles = {glm::uvec3(0, 1, 2)};
    mesh.uvs = {glm::dvec2(0, 0), glm::dvec2(1, 0), glm::dvec2(0, 1)};
    mesh.texture = cv::Mat3b(4, 4, cv::Vec3b(0, 0, 0));

    const tl::expected<mesh::Encoded, mesh::EncodeError> encode_result = mesh::encode(mesh, mesh::EncodeOptions{.texture_format = ".unknown"});

    CHECK(!encode_result.has_value());
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

            mesh::io::save_to_path(mesh, mesh_path, mesh::io::SaveOptions{.texture_format = ".png"});
            CHECK(std::filesystem::exists(mesh_path));

            const tl::expected<SimpleMesh, mesh::io::LoadMeshError> result = mesh::io::load_from_path(mesh_path);
            if (!result.has_value()) {
                FAIL(result.error().description());
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

            mesh::io::save_to_path(mesh, mesh_path, mesh::io::SaveOptions{.texture_format = ".png"});
            CHECK(std::filesystem::exists(mesh_path));

            const tl::expected<SimpleMesh, mesh::io::LoadMeshError> result = mesh::io::load_from_path(mesh_path);
            if (!result.has_value()) {
                FAIL(result.error().description());
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

            mesh::io::save_to_path(mesh, mesh_path);
            CHECK(std::filesystem::exists(mesh_path));

            const tl::expected<SimpleMesh, mesh::io::LoadMeshError> result = mesh::io::load_from_path(mesh_path);
            if (!result.has_value()) {
                FAIL(result.error().description());
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

            mesh::io::save_to_path(mesh, mesh_path);
            CHECK(std::filesystem::exists(mesh_path));

            const tl::expected<SimpleMesh, mesh::io::LoadMeshError> result = mesh::io::load_from_path(mesh_path);
            if (!result.has_value()) {
                FAIL(result.error().description());
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
