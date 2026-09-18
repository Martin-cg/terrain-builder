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

Result<std::vector<uint8_t>> save_to_buffer(const SimpleMesh &mesh, const SaveOptions& options) {
    ALP_TRY(const mesh::Encoded &encoded, mesh::encode(mesh, mesh::EncodeOptions{.texture_format = options.texture_format}));
    return ::io::write_to_bytes(encoded);
}

Result<SimpleMesh> load_from_buffer(const std::span<const uint8_t> bytes, const LoadOptions & /* options */) {
    ALP_TRY(const mesh::Encoded &encoded, ::io::read_from_bytes<mesh::Encoded>(bytes));
    ALP_TRY(mesh::Simple decoded, mesh::decode(encoded));
    return decoded;
}

Result<SimpleMesh> load_from_path(const std::filesystem::path &path, const LoadOptions & /* options */) {
    ALP_TRY(const std::vector<uint8_t> &bytes, ::io::read_bytes_from_path(path));
    ALP_TRY_CONTEXT(SimpleMesh mesh, load_from_buffer(bytes), fmt::format("decoding the mesh in {}", path.string()));
    return mesh;
}

Result<void> save_to_path(const SimpleMesh &mesh, const std::filesystem::path &path, const SaveOptions& options) {
    LOG_TRACE("Saving mesh as high precision terrain");

    ALP_TRY_CONTEXT(const std::vector<uint8_t> &bytes, save_to_buffer(mesh, options), fmt::format("encoding the mesh for {}", path.string()));
    ALP_TRY(::io::write_bytes_to_path(bytes, path));
    return {};
}

} // namespace mesh::io::terrain
