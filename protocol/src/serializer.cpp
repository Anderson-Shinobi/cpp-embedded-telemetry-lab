#include "telemetry_protocol/serializer.hpp"

#include "telemetry_protocol/crc32.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>

namespace telemetry::protocol {
namespace {

constexpr std::uint32_t byte_mask{0xFFU};

void write_u16(
    const std::span<std::uint8_t> output,
    const std::size_t offset,
    const std::uint16_t value) noexcept {
    output[offset] =
        static_cast<std::uint8_t>((value >> 8U) & byte_mask);
    output[offset + 1U] = static_cast<std::uint8_t>(value & byte_mask);
}

void write_u32(
    const std::span<std::uint8_t> output,
    const std::size_t offset,
    const std::uint32_t value) noexcept {
    output[offset] =
        static_cast<std::uint8_t>((value >> 24U) & byte_mask);
    output[offset + 1U] =
        static_cast<std::uint8_t>((value >> 16U) & byte_mask);
    output[offset + 2U] =
        static_cast<std::uint8_t>((value >> 8U) & byte_mask);
    output[offset + 3U] = static_cast<std::uint8_t>(value & byte_mask);
}

void write_u64(
    const std::span<std::uint8_t> output,
    const std::size_t offset,
    const std::uint64_t value) noexcept {
    for (std::size_t index{0U}; index < sizeof(value); ++index) {
        const std::size_t shift{(sizeof(value) - 1U - index) * 8U};
        output[offset + index] =
            static_cast<std::uint8_t>((value >> shift) & byte_mask);
    }
}

}  // namespace

SerializedFrame serialize(const TelemetryFrame& frame) noexcept {
    SerializedFrame output{};
    const std::span<std::uint8_t> bytes{output};

    output[0U] = magic[0U];
    output[1U] = magic[1U];
    output[version_offset] = static_cast<std::uint8_t>(frame.header.version);
    output[message_type_offset] =
        static_cast<std::uint8_t>(frame.header.message_type);
    write_u16(bytes, payload_size_offset, telemetry_payload_size);
    write_u32(bytes, sequence_offset, frame.header.sequence);
    write_u64(bytes, timestamp_offset, frame.header.timestamp_microseconds);
    write_u32(
        bytes,
        temperature_offset,
        std::bit_cast<std::uint32_t>(frame.payload.temperature_millicelsius));
    write_u32(bytes, pressure_offset, frame.payload.pressure_pascal);
    write_u16(bytes, supply_voltage_offset, frame.payload.supply_millivolts);
    write_u16(bytes, status_flags_offset, frame.payload.status_flags);

    const std::uint32_t crc{
        calculate_crc32(std::span<const std::uint8_t>{output}.first(crc_offset))};
    write_u32(bytes, crc_offset, crc);

    return output;
}

}  // namespace telemetry::protocol
