#include <span>
#include <vector>

#include <fmt/format.h>

#include "error/try.h"
#include "io/bytes.h"
#include "io/serialize.h"
#include "log.h"
#include "mesh/io/terrain.h"
#include "mesh/EncodedMesh.h"
#include "mesh/encode.h"

namespace mesh::io::terrain {

namespace detail {

[[nodiscard]]
Result<std::vector<uint8_t>> save_to_buffer(const SimpleMesh &mesh, const SaveOptions &options) {
    ALP_TRY(const mesh::Encoded &encoded, mesh::encode(mesh, mesh::EncodeOptions{.texture_format = options.texture_format}));
    return ::io::write_to_bytes(encoded);
}

[[nodiscard]]
Result<SimpleMesh> load_from_buffer(const std::span<const uint8_t> bytes) {
    ALP_TRY(const mesh::Encoded &encoded, ::io::read_from_bytes<mesh::Encoded>(bytes));
    ALP_TRY(mesh::Simple decoded, mesh::decode(encoded));
    return decoded;
}

} // namespace detail

Result<std::vector<uint8_t>> save_to_buffer(const SimpleMesh &mesh, const SaveOptions& options) {
    ALP_TRY_CONTEXT(std::vector<uint8_t> bytes, detail::save_to_buffer(mesh, options), "encoding a terrain mesh");
    return bytes;
}

Result<SimpleMesh> load_from_buffer(const std::span<const uint8_t> bytes, const LoadOptions & /* options */) {
    ALP_TRY_CONTEXT(SimpleMesh mesh, detail::load_from_buffer(bytes), "decoding a terrain mesh");
    return mesh;
}

Result<SimpleMesh> load_from_path(const std::filesystem::path &path, const LoadOptions & /* options */) {
    const auto context = alp::make_context("loading a terrain mesh from {}", path);
    ALP_TRY_CONTEXT(const std::vector<uint8_t> &bytes, ::io::read_bytes_from_path(path), context);
    ALP_TRY_CONTEXT(SimpleMesh mesh, detail::load_from_buffer(bytes), context);
    return mesh;
}

Result<void> save_to_path(const SimpleMesh &mesh, const std::filesystem::path &path, const SaveOptions& options) {
    LOG_TRACE("Saving mesh as high precision terrain");

    const auto context = alp::make_context("saving a terrain mesh to {}", path);
    ALP_TRY_CONTEXT(const std::vector<uint8_t> &bytes, detail::save_to_buffer(mesh, options), context);
    ALP_TRY_CONTEXT(::io::write_bytes_to_path(bytes, path), context);
    return {};
}

} // namespace mesh::io::terrain
