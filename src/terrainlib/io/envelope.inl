#pragma once

#include <zpp_bits.h>

#include <limits>

namespace io::envelope {
namespace detail {

using ArchiveByteOrder = zpp::bits::endian::little;
using ArchiveAllocationLimit = zpp::bits::alloc_limit<default_max_decompressed_size>;

template <typename Value>
tl::expected<Bytes, Error> serialize_to_bytes(const Value &value)
{
    Bytes bytes;
    zpp::bits::out output(bytes, ArchiveByteOrder{}, ArchiveAllocationLimit{});
    const zpp::bits::errc result = output(value);
    if (zpp::bits::failure(result)) {
        return tl::unexpected(Error{ErrorCode::SerializationFailed, result.code});
    }
    return bytes;
}

template <typename Value>
tl::expected<Value, Error> deserialize_from_bytes(const std::span<const std::byte> bytes)
{
    Value value{};
    zpp::bits::in input(bytes, ArchiveByteOrder{}, ArchiveAllocationLimit{});
    const zpp::bits::errc result = input(value);
    if (zpp::bits::failure(result) || input.position() != bytes.size()) {
        return tl::unexpected(Error{
            ErrorCode::DeserializationFailed,
            zpp::bits::failure(result) ? result.code : std::errc::bad_message,
        });
    }
    return value;
}

template <typename Schema, std::size_t Index, typename Current>
typename Schema::latest_type convert_to_latest(Current current)
{
    if constexpr (Index + 1 == Schema::version_count) {
        return current;
    } else {
        using Next = typename Schema::template version_at<Index + 1>::payload_type;
        return convert_to_latest<Schema, Index + 1>(Next::from_previous(std::move(current)));
    }
}

template <typename Schema, std::size_t Index = 0>
tl::expected<typename Schema::latest_type, Error> deserialize_version(
    const std::uint32_t class_version,
    const std::span<const std::byte> payload_bytes)
{
    using CurrentVersion = typename Schema::template version_at<Index>;
    if (class_version == CurrentVersion::number) {
        auto payload = deserialize_from_bytes<typename CurrentVersion::payload_type>(payload_bytes);
        if (!payload) {
            return tl::unexpected(payload.error());
        }
        return convert_to_latest<Schema, Index>(std::move(*payload));
    }

    if constexpr (Index + 1 < Schema::version_count) {
        return deserialize_version<Schema, Index + 1>(class_version, payload_bytes);
    } else {
        return tl::unexpected(Error{ErrorCode::UnsupportedClassVersion});
    }
}

} // namespace detail

template <typename Schema, std::uint32_t VersionNumber>
tl::expected<Bytes, Error> serialize(
    const typename Schema::template payload_type<VersionNumber> &payload,
    const CompressionAlgorithm compression_algorithm,
    const ChecksumAlgorithm checksum_algorithm)
{
    static_assert(Schema::template supports_version<VersionNumber>,
                  "the requested payload version is not part of the schema");

    auto payload_bytes = detail::serialize_to_bytes(payload);
    if (!payload_bytes) {
        return tl::unexpected(payload_bytes.error());
    }

    auto compressed = compress_with_checksum(*payload_bytes, compression_algorithm, checksum_algorithm);
    if (!compressed) {
        return tl::unexpected(compressed.error());
    }

    const Envelope envelope{
        .magic = magic,
        .class_name = std::string{Schema::class_name},
        .class_version = VersionNumber,
        .checksum_algorithm = checksum_algorithm,
        .checksum = std::move(compressed->checksum),
        .compression_algorithm = compression_algorithm,
        .uncompressed_size = payload_bytes->size(),
        .compressed_data = std::move(compressed->compressed_data),
    };
    return detail::serialize_to_bytes(envelope);
}

template <typename Schema>
tl::expected<typename Schema::latest_type, Error> deserialize(
    const std::span<const std::byte> bytes,
    const std::size_t max_decompressed_size)
{
    auto envelope = detail::deserialize_from_bytes<Envelope>(bytes);
    if (!envelope) {
        return tl::unexpected(envelope.error());
    }
    if (envelope->magic != magic) {
        return tl::unexpected(Error{ErrorCode::InvalidMagic});
    }
    if (envelope->class_name != Schema::class_name) {
        return tl::unexpected(Error{ErrorCode::WrongClassName});
    }
    if (!Schema::supports_version_number(envelope->class_version)) {
        return tl::unexpected(Error{ErrorCode::UnsupportedClassVersion});
    }

    const std::size_t effective_max_decompressed_size =
        std::min(max_decompressed_size, default_max_decompressed_size);
    if (envelope->uncompressed_size > std::numeric_limits<std::size_t>::max()
        || envelope->uncompressed_size > effective_max_decompressed_size) {
        return tl::unexpected(Error{ErrorCode::SizeLimitExceeded});
    }

    auto payload_bytes = checked_decompress(
        envelope->compressed_data,
        envelope->compression_algorithm,
        envelope->checksum_algorithm,
        envelope->checksum,
        static_cast<std::size_t>(envelope->uncompressed_size));
    if (!payload_bytes) {
        if (payload_bytes.error().code == ErrorCode::SizeLimitExceeded) {
            return tl::unexpected(Error{ErrorCode::DecompressionFailed});
        }
        return tl::unexpected(payload_bytes.error());
    }
    if (payload_bytes->size() != envelope->uncompressed_size) {
        return tl::unexpected(Error{ErrorCode::DecompressionFailed});
    }

    return detail::deserialize_version<Schema>(envelope->class_version, *payload_bytes);
}

} // namespace io::envelope
