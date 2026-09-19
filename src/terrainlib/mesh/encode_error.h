#pragma once

#include <string_view>

namespace mesh {

// A mesh that could not be encoded.
enum class EncodeError {
    PositionEncode,
    UvEncode,
    TriangleEncode,
    TextureEncode
};

inline std::string_view format_as(const EncodeError error) {
    switch (error) {
    case EncodeError::PositionEncode:
        return "encoding the positions failed";
    case EncodeError::UvEncode:
        return "encoding the UVs failed";
    case EncodeError::TriangleEncode:
        return "encoding the triangles failed";
    case EncodeError::TextureEncode:
        return "encoding the texture failed";
    default:
        return "an unknown encoding failure";
    }
}

// An encoded mesh that could not be decoded.
enum class DecodeError {
    IncompatibleData,
    PositionDecode,
    UvDecode,
    TriangleDecode,
    TextureDecode
};

inline std::string_view format_as(const DecodeError error) {
    switch (error) {
    case DecodeError::IncompatibleData:
        return "the data is incompatible";
    case DecodeError::PositionDecode:
        return "decoding the positions failed";
    case DecodeError::UvDecode:
        return "decoding the UVs failed";
    case DecodeError::TriangleDecode:
        return "decoding the triangles failed";
    case DecodeError::TextureDecode:
        return "decoding the texture failed";
    default:
        return "an unknown decoding failure";
    }
}

} // namespace mesh