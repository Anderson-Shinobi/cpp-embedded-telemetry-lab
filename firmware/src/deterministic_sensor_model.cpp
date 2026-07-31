#include "telemetry_firmware/deterministic_sensor_model.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace telemetry::firmware {
namespace {

constexpr std::uint16_t flag_value(
    const telemetry::protocol::StatusFlag flag) noexcept {
    return static_cast<std::uint16_t>(flag);
}

constexpr std::uint16_t combine_flags(
    const telemetry::protocol::StatusFlag first,
    const telemetry::protocol::StatusFlag second) noexcept {
    return static_cast<std::uint16_t>(flag_value(first) | flag_value(second));
}

constexpr std::array<telemetry::protocol::TelemetrySample,
                     deterministic_sample_count>
    samples{{
        {-5'500, 98'500U, 2'900U, flag_value(telemetry::protocol::StatusFlag::none)},
        {0, 100'000U, 3'300U,
         flag_value(telemetry::protocol::StatusFlag::sensor_warning)},
        {12'500, 100'800U, 3'150U,
         flag_value(telemetry::protocol::StatusFlag::voltage_warning)},
        {23'000, 101'325U, 3'300U,
         flag_value(telemetry::protocol::StatusFlag::communication_warning)},
        {31'500, 102'100U, 3'250U,
         flag_value(telemetry::protocol::StatusFlag::watchdog_event)},
        {45'000, 99'500U, 2'800U,
         combine_flags(
             telemetry::protocol::StatusFlag::sensor_warning,
             telemetry::protocol::StatusFlag::voltage_warning)},
        {18'500, 100'500U, 3'000U,
         combine_flags(
             telemetry::protocol::StatusFlag::communication_warning,
             telemetry::protocol::StatusFlag::watchdog_event)},
        {27'000, 101'800U, 3'350U,
         flag_value(telemetry::protocol::StatusFlag::none)},
    }};

}  // namespace

bool DeterministicSensorModel::has_next() const noexcept {
    return next_index_ < samples.size();
}

telemetry::protocol::TelemetryFrame DeterministicSensorModel::next() noexcept {
    if (!has_next()) {
        return {};
    }

    const std::size_t current_index{next_index_};
    ++next_index_;

    return telemetry::protocol::TelemetryFrame{
        .header =
            {
                .version = telemetry::protocol::supported_version,
                .message_type = telemetry::protocol::supported_message_type,
                .payload_size = telemetry::protocol::telemetry_payload_size,
                .sequence =
                    initial_sequence + static_cast<std::uint32_t>(current_index),
                .timestamp_microseconds =
                    initial_timestamp_microseconds +
                    (static_cast<std::uint64_t>(current_index) *
                     timestamp_increment_microseconds),
            },
        .payload = samples[current_index],
    };
}

std::size_t DeterministicSensorModel::remaining() const noexcept {
    return samples.size() - next_index_;
}

}  // namespace telemetry::firmware
