#include "telemetry_protocol/deserializer.hpp"

#include "telemetry_protocol/crc32.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace telemetry::protocol {
namespace {

std::uint16_t read_u16(
    const std::span<const std::uint8_t> bytes,
    const std::size_t offset) noexcept {
    const std::uint32_t high{
        static_cast<std::uint32_t>(bytes[offset]) << 8U};
    const std::uint32_t low{
        static_cast<std::uint32_t>(bytes[offset + 1U])};
    return static_cast<std::uint16_t>(high | low);
}

std::uint32_t read_u32(
    const std::span<const std::uint8_t> bytes,
    const std::size_t offset) noexcept {
    std::uint32_t value{0U};
    for (std::size_t index{0U}; index < sizeof(value); ++index) {
        value = (value << 8U) |
                static_cast<std::uint32_t>(bytes[offset + index]);
    }
    return value;
}

std::uint64_t read_u64(
    const std::span<const std::uint8_t> bytes,
    const std::size_t offset) noexcept {
    std::uint64_t value{0U};
    for (std::size_t index{0U}; index < sizeof(value); ++index) {
        value = (value << 8U) |
                static_cast<std::uint64_t>(bytes[offset + index]);
    }
    return value;
}

ParseResult failure(const ParseError error) noexcept {
    return ParseResult{std::nullopt, error};
}

}  // namespace

bool ParseResult::has_value() const noexcept {
    return frame.has_value();
}

ParseResult deserialize(const std::span<const std::uint8_t> bytes) noexcept {
    if (bytes.size() != frame_size) {
        return failure(ParseError::invalid_frame_size);
    }

    if (bytes[0U] != magic[0U] || bytes[1U] != magic[1U]) {
        return failure(ParseError::invalid_magic);
    }

    if (bytes[version_offset] !=
        static_cast<std::uint8_t>(supported_version)) {
        return failure(ParseError::unsupported_version);
    }

    if (bytes[message_type_offset] !=
        static_cast<std::uint8_t>(supported_message_type)) {
        return failure(ParseError::unsupported_message_type);
    }

    const std::uint16_t parsed_payload_size{
        read_u16(bytes, payload_size_offset)};
    if (parsed_payload_size != telemetry_payload_size) {
        return failure(ParseError::invalid_payload_size);
    }

    const std::uint32_t expected_crc{
        calculate_crc32(bytes.first(crc_offset))};
    const std::uint32_t received_crc{read_u32(bytes, crc_offset)};
    if (received_crc != expected_crc) {
        return failure(ParseError::checksum_mismatch);
    }

    TelemetryFrame frame{
        .header =
            {
                .version = supported_version,
                .message_type = supported_message_type,
                .payload_size = parsed_payload_size,
                .sequence = read_u32(bytes, sequence_offset),
                .timestamp_microseconds = read_u64(bytes, timestamp_offset),
            },
        .payload =
            {
                .temperature_millicelsius = std::bit_cast<std::int32_t>(
                    read_u32(bytes, temperature_offset)),
                .pressure_pascal = read_u32(bytes, pressure_offset),
                .supply_millivolts = read_u16(bytes, supply_voltage_offset),
                .status_flags = read_u16(bytes, status_flags_offset),
            },
    };

    return ParseResult{frame, ParseError::none};
}

}  // namespace telemetry::protocol
