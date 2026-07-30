#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace telemetry::protocol {

enum class ProtocolVersion : std::uint8_t {
    v1 = 1U,
};

enum class MessageType : std::uint8_t {
    telemetry_sample = 1U,
};

enum class StatusFlag : std::uint16_t {
    none = 0U,
    sensor_warning = 1U << 0U,
    voltage_warning = 1U << 1U,
    communication_warning = 1U << 2U,
    watchdog_event = 1U << 3U,
};

struct TelemetrySample {
    std::int32_t temperature_millicelsius{};
    std::uint32_t pressure_pascal{};
    std::uint16_t supply_millivolts{};
    std::uint16_t status_flags{};

    [[nodiscard]] bool operator==(const TelemetrySample&) const noexcept = default;
};

struct FrameHeader {
    ProtocolVersion version{ProtocolVersion::v1};
    MessageType message_type{MessageType::telemetry_sample};
    std::uint16_t payload_size{};
    std::uint32_t sequence{};
    std::uint64_t timestamp_microseconds{};

    [[nodiscard]] bool operator==(const FrameHeader&) const noexcept = default;
};

struct TelemetryFrame {
    FrameHeader header{};
    TelemetrySample payload{};

    [[nodiscard]] bool operator==(const TelemetryFrame&) const noexcept = default;
};

inline constexpr std::array<std::uint8_t, 2U> magic{0x54U, 0x4CU};
inline constexpr std::size_t magic_size{magic.size()};
inline constexpr std::size_t header_size{18U};
inline constexpr std::uint16_t telemetry_payload_size{12U};
inline constexpr std::size_t crc_size{4U};
inline constexpr std::size_t frame_size{
    header_size + static_cast<std::size_t>(telemetry_payload_size) + crc_size};
inline constexpr ProtocolVersion supported_version{ProtocolVersion::v1};
inline constexpr MessageType supported_message_type{MessageType::telemetry_sample};

inline constexpr std::size_t version_offset{2U};
inline constexpr std::size_t message_type_offset{3U};
inline constexpr std::size_t payload_size_offset{4U};
inline constexpr std::size_t sequence_offset{6U};
inline constexpr std::size_t timestamp_offset{10U};
inline constexpr std::size_t temperature_offset{18U};
inline constexpr std::size_t pressure_offset{22U};
inline constexpr std::size_t supply_voltage_offset{26U};
inline constexpr std::size_t status_flags_offset{28U};
inline constexpr std::size_t crc_offset{30U};

static_assert(frame_size == 34U);
static_assert(crc_offset + crc_size == frame_size);

}  // namespace telemetry::protocol
