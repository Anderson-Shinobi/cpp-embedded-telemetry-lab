#pragma once

#include "telemetry_protocol/deserializer.hpp"
#include "telemetry_protocol/protocol_types.hpp"

#include <atomic>
#include <cstdint>
#include <optional>

namespace telemetry::host {

struct ParseErrorCounters {
    std::uint64_t invalid_frame_size{};
    std::uint64_t invalid_magic{};
    std::uint64_t unsupported_version{};
    std::uint64_t unsupported_message_type{};
    std::uint64_t invalid_payload_size{};
    std::uint64_t checksum_mismatch{};

    [[nodiscard]] bool operator==(const ParseErrorCounters&) const noexcept =
        default;
};

struct TelemetryMetricsSnapshot {
    std::uint64_t inputs_received{};
    std::uint64_t frames_accepted{};
    std::uint64_t frames_rejected{};
    std::uint64_t queue_closed_rejections{};
    std::uint64_t frames_processed{};
    ParseErrorCounters parse_errors{};

    std::optional<std::uint32_t> first_sequence{};
    std::optional<std::uint32_t> last_sequence{};
    std::uint64_t missing_sequences{};
    std::uint64_t duplicate_sequences{};
    std::uint64_t out_of_order_sequences{};

    std::uint64_t sensor_warnings{};
    std::uint64_t voltage_warnings{};
    std::uint64_t communication_warnings{};
    std::uint64_t watchdog_events{};

    std::optional<std::int32_t> minimum_temperature_millicelsius{};
    std::optional<std::int32_t> maximum_temperature_millicelsius{};
    std::int64_t temperature_sum_millicelsius{};

    std::optional<std::uint16_t> minimum_supply_millivolts{};
    std::optional<std::uint16_t> maximum_supply_millivolts{};
    std::uint64_t supply_sum_millivolts{};

    [[nodiscard]] bool operator==(
        const TelemetryMetricsSnapshot&) const noexcept = default;
};

class TelemetryMetrics final {
public:
    void record_input() noexcept;
    void record_accepted() noexcept;
    void record_parse_rejected(
        telemetry::protocol::ParseError error) noexcept;
    void record_queue_closed() noexcept;
    void record_processed(
        const telemetry::protocol::TelemetryFrame& frame) noexcept;

    [[nodiscard]] TelemetryMetricsSnapshot snapshot() const noexcept;

private:
    std::atomic<std::uint64_t> inputs_received_{0U};
    std::atomic<std::uint64_t> frames_accepted_{0U};
    std::atomic<std::uint64_t> frames_rejected_{0U};
    std::atomic<std::uint64_t> queue_closed_rejections_{0U};
    std::atomic<std::uint64_t> invalid_frame_size_{0U};
    std::atomic<std::uint64_t> invalid_magic_{0U};
    std::atomic<std::uint64_t> unsupported_version_{0U};
    std::atomic<std::uint64_t> unsupported_message_type_{0U};
    std::atomic<std::uint64_t> invalid_payload_size_{0U};
    std::atomic<std::uint64_t> checksum_mismatch_{0U};

    std::uint64_t frames_processed_{};
    std::optional<std::uint32_t> first_sequence_{};
    std::optional<std::uint32_t> last_sequence_{};
    std::uint64_t missing_sequences_{};
    std::uint64_t duplicate_sequences_{};
    std::uint64_t out_of_order_sequences_{};

    std::uint64_t sensor_warnings_{};
    std::uint64_t voltage_warnings_{};
    std::uint64_t communication_warnings_{};
    std::uint64_t watchdog_events_{};

    std::optional<std::int32_t> minimum_temperature_millicelsius_{};
    std::optional<std::int32_t> maximum_temperature_millicelsius_{};
    std::int64_t temperature_sum_millicelsius_{};

    std::optional<std::uint16_t> minimum_supply_millivolts_{};
    std::optional<std::uint16_t> maximum_supply_millivolts_{};
    std::uint64_t supply_sum_millivolts_{};
};

}  // namespace telemetry::host
