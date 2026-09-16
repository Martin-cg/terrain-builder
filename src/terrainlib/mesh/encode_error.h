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
        return "Position encoding failed";
    case EncodeError::UvEncode:
        return "UV encoding failed";
    case EncodeError::TriangleEncode:
        return "Triangle encoding failed";
    case EncodeError::TextureEncode:
        return "Texture encoding failed";
    default:
        return "Unknown EncodeError";
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
        return "Incompatible data";
    case DecodeError::PositionDecode:
        return "Position decoding failed";
    case DecodeError::UvDecode:
        return "UV decoding failed";
    case DecodeError::TriangleDecode:
        return "Triangle decoding failed";
    case DecodeError::TextureDecode:
        return "Texture decoding failed";
    default:
        return "Unknown DecodeError";
    }
}

} // namespace mesh