#include "telemetry_host/simulated_frame_source.hpp"

#include "telemetry_protocol/protocol_types.hpp"
#include "telemetry_protocol/serializer.hpp"

#include <cstdint>
#include <utility>
#include <vector>

namespace telemetry::host {
namespace {

using telemetry::protocol::MessageType;
using telemetry::protocol::ProtocolVersion;
using telemetry::protocol::StatusFlag;
using telemetry::protocol::TelemetryFrame;

[[nodiscard]] std::uint16_t flag_value(const StatusFlag flag) noexcept {
    return static_cast<std::uint16_t>(flag);
}

[[nodiscard]] RawFrame make_raw_frame(
    const std::uint32_t sequence,
    const std::int32_t temperature,
    const std::uint16_t supply,
    const std::uint16_t flags) {
    const TelemetryFrame frame{
        .header =
            {
                .version = ProtocolVersion::v1,
                .message_type = MessageType::telemetry_sample,
                .payload_size = telemetry::protocol::telemetry_payload_size,
                .sequence = sequence,
                .timestamp_microseconds =
                    1'000'000ULL + static_cast<std::uint64_t>(sequence),
            },
        .payload =
            {
                .temperature_millicelsius = temperature,
                .pressure_pascal = 100'000U + sequence,
                .supply_millivolts = supply,
                .status_flags = flags,
            },
    };
    const telemetry::protocol::SerializedFrame serialized{
        telemetry::protocol::serialize(frame)};
    return RawFrame{serialized.begin(), serialized.end()};
}

}  // namespace

std::vector<RawFrame> SimulatedFrameSource::generate() const {
    const std::uint16_t sensor{flag_value(StatusFlag::sensor_warning)};
    const std::uint16_t voltage{flag_value(StatusFlag::voltage_warning)};
    const std::uint16_t communication{
        flag_value(StatusFlag::communication_warning)};
    const std::uint16_t watchdog{flag_value(StatusFlag::watchdog_event)};

    RawFrame valid_100{make_raw_frame(100U, -5'000, 3'000U, sensor)};
    RawFrame valid_101{make_raw_frame(101U, 0, 3'100U, voltage)};
    RawFrame valid_103_communication{
        make_raw_frame(103U, 10'000, 3'200U, communication)};
    RawFrame valid_103_watchdog{
        make_raw_frame(103U, 20'000, 3'300U, watchdog)};
    RawFrame valid_102{
        make_raw_frame(102U, -10'000, 2'900U, sensor | voltage)};
    RawFrame valid_104{
        make_raw_frame(104U, 30'000, 3'400U, communication | watchdog)};
    RawFrame valid_105{make_raw_frame(
        105U,
        5'000,
        3'500U,
        flag_value(StatusFlag::none))};
    RawFrame valid_106{make_raw_frame(
        106U,
        15'000,
        3'600U,
        sensor | voltage | communication | watchdog)};

    RawFrame invalid_magic{valid_100};
    invalid_magic[0U] = 0x00U;

    RawFrame invalid_version{valid_101};
    invalid_version[telemetry::protocol::version_offset] = 0x02U;

    RawFrame invalid_type{valid_103_communication};
    invalid_type[telemetry::protocol::message_type_offset] = 0x02U;

    RawFrame invalid_payload_size{valid_103_watchdog};
    invalid_payload_size[telemetry::protocol::payload_size_offset + 1U] =
        0x0BU;

    RawFrame corrupted_payload{valid_104};
    corrupted_payload[telemetry::protocol::temperature_offset] ^= 0x01U;

    RawFrame corrupted_crc{valid_105};
    corrupted_crc[telemetry::protocol::crc_offset +
                  telemetry::protocol::crc_size - 1U] ^= 0x01U;

    RawFrame truncated{valid_106};
    truncated.pop_back();

    std::vector<RawFrame> frames{};
    frames.reserve(simulated_input_count);
    frames.push_back(std::move(valid_100));
    frames.push_back(std::move(invalid_magic));
    frames.push_back(std::move(valid_101));
    frames.push_back(std::move(invalid_version));
    frames.push_back(std::move(valid_103_communication));
    frames.push_back(std::move(invalid_type));
    frames.push_back(std::move(valid_103_watchdog));
    frames.push_back(std::move(invalid_payload_size));
    frames.push_back(std::move(valid_102));
    frames.push_back(std::move(corrupted_payload));
    frames.push_back(std::move(valid_104));
    frames.push_back(std::move(corrupted_crc));
    frames.push_back(std::move(valid_105));
    frames.push_back(std::move(truncated));
    frames.push_back(std::move(valid_106));
    return frames;
}

}  // namespace telemetry::host
