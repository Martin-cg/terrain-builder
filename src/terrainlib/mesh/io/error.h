#pragma once

#include <filesystem>
#include <string_view>

#include <cgltf.h>
#include <fmt/format.h>
#include <libassert/assert.hpp>

#include "error/Error.h"
#include "error/Result.h"
#include "error/format.h"
#include "io/Error.h"
#include "mesh/encode_error.h"

namespace mesh::io {

// A glTF operation that cgltf reported as failed.
class GltfError {
public:
    explicit GltfError(const cgltf_result result) : _result(result) {
        ASSERT(result != cgltf_result_success);
    }

    bool operator==(const GltfError &) const = default;

private:
    friend std::string_view format_as(GltfError error);

    cgltf_result _result;
};

// What cgltf's result means.
inline std::string_view format_as(const GltfError error) {
    switch (error._result) {
    case cgltf_result_data_too_short:
        return "the data is too short";
    case cgltf_result_unknown_format:
        return "the format is unknown";
    case cgltf_result_invalid_json:
        return "the JSON is invalid";
    case cgltf_result_invalid_gltf:
        return "the glTF is invalid";
    case cgltf_result_invalid_options:
        return "the options are invalid";
    case cgltf_result_file_not_found:
        return "the file was not found";
    case cgltf_result_io_error:
        return "reading or writing the file failed";
    case cgltf_result_out_of_memory:
        return "cgltf ran out of memory";
    case cgltf_result_legacy_gltf:
        return "the file is legacy glTF";
    default:
        return "an unknown glTF failure";
    }
}

// A path whose extension names no mesh format this library reads or writes.
struct UnsupportedFormat {
    std::filesystem::path extension;

    bool operator==(const UnsupportedFormat &) const = default;
};

// A glTF file whose content this library cannot read as a mesh.
struct UnsupportedGltf {
    bool operator==(const UnsupportedGltf &) const = default;
};

inline std::string_view format_as(const UnsupportedGltf) {
    return "the glTF content is not supported";
}

} // namespace mesh::io

template <>
struct fmt::formatter<mesh::io::UnsupportedFormat> {
    constexpr fmt::format_parse_context::iterator parse(fmt::format_parse_context &context) {
        return context.begin();
    }

    fmt::format_context::iterator format(const mesh::io::UnsupportedFormat &error, fmt::format_context &context) const {
        return fmt::format_to(context.out(), "unsupported format {}", error.extension.string());
    }
};

namespace mesh::io {

// An error from loading or saving a mesh.
using Error = alp::Error<::io::Error, EncodeError, DecodeError, GltfError, UnsupportedFormat, UnsupportedGltf>;

// A value, or an error from loading or saving a mesh.
template <typename T>
using Result = alp::Result<T, Error>;

} // namespace mesh::io
