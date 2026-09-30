#pragma once

#include <string_view>

#include "error/Error.h"
#include "io/UnsupportedFormat.h"
#include "texture/io.h"

namespace mesh {

// The part of a mesh's geometry that could not be encoded.
enum class GeometryEncodeError {
    Positions,
    Uvs,
    Triangles
};

inline std::string_view format_as(const GeometryEncodeError error) {
    switch (error) {
    case GeometryEncodeError::Positions:
        return "encoding the positions failed";
    case GeometryEncodeError::Uvs:
        return "encoding the UVs failed";
    case GeometryEncodeError::Triangles:
        return "encoding the triangles failed";
    default:
        return "an unknown encoding failure";
    }
}

// The part of an encoded mesh's geometry that could not be decoded.
enum class GeometryDecodeError {
    IncompatibleData,
    Positions,
    Uvs,
    Triangles
};

inline std::string_view format_as(const GeometryDecodeError error) {
    switch (error) {
    case GeometryDecodeError::IncompatibleData:
        return "the data is incompatible";
    case GeometryDecodeError::Positions:
        return "decoding the positions failed";
    case GeometryDecodeError::Uvs:
        return "decoding the UVs failed";
    case GeometryDecodeError::Triangles:
        return "decoding the triangles failed";
    default:
        return "an unknown decoding failure";
    }
}

// An error from encoding a mesh.
using EncodeError = alp::Error<GeometryEncodeError, ::io::UnsupportedFormat, texture::io::EncodeError>;

// An error from decoding a mesh.
using DecodeError = alp::Error<GeometryDecodeError, texture::io::DecodeError>;

} // namespace mesh
