#pragma once

#include <ostream>
#include <span>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include <tl/expected.hpp>

#include "log.h"
#include "meshopt/encode.h"
#include "mesh/EncodedMesh.h"
#include "mesh/SimpleMesh.h"
#include "mesh/io/texture.h"

namespace mesh {

namespace {
constexpr uint32_t GL_BYTE = 0x1400;
constexpr uint32_t GL_UNSIGNED_BYTE = 0x1401;
constexpr uint32_t GL_SHORT = 0x1402;
constexpr uint32_t GL_UNSIGNED_SHORT = 0x1403;
constexpr uint32_t GL_INT = 0x1404;
constexpr uint32_t GL_UNSIGNED_INT = 0x1405;
constexpr uint32_t GL_FLOAT = 0x1406;
constexpr uint32_t GL_DOUBLE = 0x140A;
}

template <typename T>
constexpr uint32_t component_type_id() {
    if constexpr (std::is_same_v<T, float>) {
        return GL_FLOAT;
    } else if constexpr (std::is_same_v<T, double>) {
        return GL_DOUBLE;
    } else if constexpr (std::is_same_v<T, int>) {
        return GL_INT;
    } else if constexpr (std::is_same_v<T, unsigned int>) {
        return GL_UNSIGNED_INT;
    } else if constexpr (std::is_same_v<T, short>) {
        return GL_SHORT;
    } else if constexpr (std::is_same_v<T, unsigned short>) {
        return GL_UNSIGNED_SHORT;
    } else if constexpr (std::is_same_v<T, char>) {
        return GL_BYTE;
    } else if constexpr (std::is_same_v<T, unsigned char>) {
        return GL_UNSIGNED_BYTE;
    } else {
        return -1;
    }
}

struct EncodeOptions {
    // Texture format
    std::string texture_format = ".jpeg";
};

// Why encoding failed: the texture could not be encoded.
struct EncodeError {
    std::string message; // OpenCV's description
};

inline std::ostream& operator<<(std::ostream& os, const EncodeError& err) {
    return os << "Texture encoding failed: " << err.message;
}

template <glm::length_t n_dims, typename T>
tl::expected<Encoded, EncodeError> encode(const Simple_<n_dims, T>& mesh, const EncodeOptions options = {}) {
    const size_t vertex_count = mesh.vertex_count();
    const size_t face_count = mesh.face_count();

    // Encode positions
    std::vector<uint8_t> position_buf;
    if (vertex_count > 0) {
        position_buf = meshopt::encode_vertex_buffer(mesh.positions);
    }

    // Encode uvs
    std::vector<uint8_t> uv_buf;
    if (mesh.has_uvs()) {
        uv_buf = meshopt::encode_vertex_buffer(mesh.uvs);
    }

    // Encode triangles
    std::vector<uint8_t> index_buf;
    if (face_count > 0) {
        index_buf = meshopt::encode_index_buffer(mesh.triangles);
    }

    // Encode texture
    std::vector<uint8_t> texture_buf;
    if (mesh.has_texture()) {
        try {
            texture_buf = mesh::io::write_texture_to_encoded_buffer(mesh.texture.value(), options.texture_format);
        } catch (const cv::Exception &e) {
            return tl::unexpected(EncodeError{e.what()});
        }
    }

    const Encoded::Header header{
        .version = 1,
        .n_dims = n_dims,
        .component_type = component_type_id<T>(),
        .vertex_count = static_cast<uint32_t>(vertex_count),
        .face_count = static_cast<uint32_t>(mesh.face_count())};

    return Encoded{
        .header = header,
        .triangles = index_buf,
        .positions = position_buf,
        .uvs = uv_buf,
        .texture = texture_buf};
}

struct DecodeOptions {
};

// The encoded header is for another version, dimension count or component type.
struct IncompatibleData {};

// A stream of an encoded mesh.
enum class MeshStream {
    Positions,
    Uvs,
    Triangles
};

inline std::ostream &operator<<(std::ostream &os, const MeshStream stream) {
    switch (stream) {
    case MeshStream::Positions:
        return os << "positions";
    case MeshStream::Uvs:
        return os << "uvs";
    case MeshStream::Triangles:
        return os << "triangles";
    }
    return os << "unknown stream";
}

// A vertex or index stream that failed to decode.
struct StreamDecodeError {
    MeshStream stream;
    meshopt::DecodeError error;
};

// The texture could not be decoded.
struct TextureDecodeError {
    std::string message; // OpenCV's description
};

using DecodeError = std::variant<IncompatibleData, StreamDecodeError, TextureDecodeError>;

inline std::ostream &operator<<(std::ostream &os, const DecodeError &err) {
    if (std::holds_alternative<IncompatibleData>(err)) {
        return os << "Incompatible data";
    }
    if (const StreamDecodeError *stream_error = std::get_if<StreamDecodeError>(&err)) {
        return os << "Decoding the " << stream_error->stream << " failed with meshopt code " << stream_error->error.code;
    }
    const TextureDecodeError &texture_error = std::get<TextureDecodeError>(err);
    return os << "Texture decoding failed: " << texture_error.message;
}


template <glm::length_t n_dims = 3, typename T = double>
tl::expected<Simple_<n_dims, T>, DecodeError> decode(const Encoded &encoded, const DecodeOptions = {}) {
    const uint32_t expected_component_type = component_type_id<T>();
    const Encoded::Header& header = encoded.header;
    if (header.version != 1 ||
        header.n_dims != n_dims ||
        header.component_type != expected_component_type) {
        return tl::unexpected(IncompatibleData{});
    }

    using Mesh = Simple_<n_dims, T>;
    Mesh mesh;

    // Decode positions
    const size_t vertex_count = header.vertex_count;
    mesh.positions.resize(vertex_count);
    const tl::expected<void, meshopt::DecodeError> positions_result = meshopt::try_decode_vertex_buffer(std::span(mesh.positions), encoded.positions);
    if (!positions_result) {
        return tl::unexpected(StreamDecodeError{MeshStream::Positions, positions_result.error()});
    }

    // Decode uvs
    if (!encoded.uvs.empty()) {
        mesh.uvs.resize(vertex_count);
        const tl::expected<void, meshopt::DecodeError> uvs_result = meshopt::try_decode_vertex_buffer(std::span(mesh.uvs), encoded.uvs);
        if (!uvs_result) {
            return tl::unexpected(StreamDecodeError{MeshStream::Uvs, uvs_result.error()});
        }
    }

    // Decode triangles
    const size_t face_count = header.face_count;
    mesh.triangles.resize(face_count);
    const tl::expected<void, meshopt::DecodeError> triangles_result = meshopt::try_decode_index_buffer(std::span(mesh.triangles), encoded.triangles);
    if (!triangles_result) {
        return tl::unexpected(StreamDecodeError{MeshStream::Triangles, triangles_result.error()});
    }

    // Decode texture
    if (!encoded.texture.empty()) {
        try {
            cv::Mat texture = mesh::io::read_texture_from_encoded_bytes(encoded.texture);
            // OpenCV reports bytes it cannot read as an image by returning an empty matrix, not by throwing.
            if (texture.empty()) {
                return tl::unexpected(TextureDecodeError{"the bytes are not an image OpenCV can read"});
            }
            mesh.texture = std::move(texture);
        } catch (const cv::Exception &e) {
            return tl::unexpected(TextureDecodeError{e.what()});
        }
    }

    return mesh;
}
}
