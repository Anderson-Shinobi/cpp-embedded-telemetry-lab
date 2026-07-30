#include "telemetry_host/telemetry_metrics.hpp"

#include <algorithm>
#include <atomic>
#include <cstdint>

namespace telemetry::host {
namespace {

[[nodiscard]] bool has_flag(
    const std::uint16_t flags,
    const telemetry::protocol::StatusFlag flag) noexcept {
    return (flags & static_cast<std::uint16_t>(flag)) != 0U;
}

}  // namespace

void TelemetryMetrics::record_input() noexcept {
    inputs_received_.fetch_add(1U, std::memory_order_relaxed);
}

void TelemetryMetrics::record_accepted() noexcept {
    frames_accepted_.fetch_add(1U, std::memory_order_relaxed);
}

void TelemetryMetrics::record_parse_rejected(
    const telemetry::protocol::ParseError error) noexcept {
    frames_rejected_.fetch_add(1U, std::memory_order_relaxed);

    using telemetry::protocol::ParseError;
    switch (error) {
        case ParseError::invalid_frame_size:
            invalid_frame_size_.fetch_add(1U, std::memory_order_relaxed);
            break;
        case ParseError::invalid_magic:
            invalid_magic_.fetch_add(1U, std::memory_order_relaxed);
            break;
        case ParseError::unsupported_version:
            unsupported_version_.fetch_add(1U, std::memory_order_relaxed);
            break;
        case ParseError::unsupported_message_type:
            unsupported_message_type_.fetch_add(
                1U, std::memory_order_relaxed);
            break;
        case ParseError::invalid_payload_size:
            invalid_payload_size_.fetch_add(1U, std::memory_order_relaxed);
            break;
        case ParseError::checksum_mismatch:
            checksum_mismatch_.fetch_add(1U, std::memory_order_relaxed);
            break;
        case ParseError::none:
            break;
    }
}

void TelemetryMetrics::record_queue_closed() noexcept {
    frames_rejected_.fetch_add(1U, std::memory_order_relaxed);
    queue_closed_rejections_.fetch_add(1U, std::memory_order_relaxed);
}

void TelemetryMetrics::record_processed(
    const telemetry::protocol::TelemetryFrame& frame) noexcept {
    ++frames_processed_;

    const std::uint32_t sequence{frame.header.sequence};
    if (!first_sequence_.has_value()) {
        first_sequence_ = sequence;
        last_sequence_ = sequence;
    } else if (sequence == *last_sequence_) {
        ++duplicate_sequences_;
    } else if (sequence > *last_sequence_) {
        const std::uint32_t distance{sequence - *last_sequence_};
        if (distance > 1U) {
            missing_sequences_ += static_cast<std::uint64_t>(distance - 1U);
        }
        last_sequence_ = sequence;
    } else {
        ++out_of_order_sequences_;
    }

    const std::uint16_t flags{frame.payload.status_flags};
    if (has_flag(flags, telemetry::protocol::StatusFlag::sensor_warning)) {
        ++sensor_warnings_;
    }
    if (has_flag(flags, telemetry::protocol::StatusFlag::voltage_warning)) {
        ++voltage_warnings_;
    }
    if (has_flag(
            flags, telemetry::protocol::StatusFlag::communication_warning)) {
        ++communication_warnings_;
    }
    if (has_flag(flags, telemetry::protocol::StatusFlag::watchdog_event)) {
        ++watchdog_events_;
    }

    const std::int32_t temperature{
        frame.payload.temperature_millicelsius};
    if (!minimum_temperature_millicelsius_.has_value()) {
        minimum_temperature_millicelsius_ = temperature;
        maximum_temperature_millicelsius_ = temperature;
    } else {
        minimum_temperature_millicelsius_ =
            std::min(*minimum_temperature_millicelsius_, temperature);
        maximum_temperature_millicelsius_ =
            std::max(*maximum_temperature_millicelsius_, temperature);
    }
    temperature_sum_millicelsius_ += static_cast<std::int64_t>(temperature);

    const std::uint16_t supply{frame.payload.supply_millivolts};
    if (!minimum_supply_millivolts_.has_value()) {
        minimum_supply_millivolts_ = supply;
        maximum_supply_millivolts_ = supply;
    } else {
        minimum_supply_millivolts_ =
            std::min(*minimum_supply_millivolts_, supply);
        maximum_supply_millivolts_ =
            std::max(*maximum_supply_millivolts_, supply);
    }
    supply_sum_millivolts_ += static_cast<std::uint64_t>(supply);
}

TelemetryMetricsSnapshot TelemetryMetrics::snapshot() const noexcept {
    return TelemetryMetricsSnapshot{
        .inputs_received =
            inputs_received_.load(std::memory_order_relaxed),
        .frames_accepted =
            frames_accepted_.load(std::memory_order_relaxed),
        .frames_rejected =
            frames_rejected_.load(std::memory_order_relaxed),
        .queue_closed_rejections =
            queue_closed_rejections_.load(std::memory_order_relaxed),
        .frames_processed = frames_processed_,
        .parse_errors =
            {
                .invalid_frame_size =
                    invalid_frame_size_.load(std::memory_order_relaxed),
                .invalid_magic =
                    invalid_magic_.load(std::memory_order_relaxed),
                .unsupported_version =
                    unsupported_version_.load(std::memory_order_relaxed),
                .unsupported_message_type =
                    unsupported_message_type_.load(std::memory_order_relaxed),
                .invalid_payload_size =
                    invalid_payload_size_.load(std::memory_order_relaxed),
                .checksum_mismatch =
                    checksum_mismatch_.load(std::memory_order_relaxed),
            },
        .first_sequence = first_sequence_,
        .last_sequence = last_sequence_,
        .missing_sequences = missing_sequences_,
        .duplicate_sequences = duplicate_sequences_,
        .out_of_order_sequences = out_of_order_sequences_,
        .sensor_warnings = sensor_warnings_,
        .voltage_warnings = voltage_warnings_,
        .communication_warnings = communication_warnings_,
        .watchdog_events = watchdog_events_,
        .minimum_temperature_millicelsius =
            minimum_temperature_millicelsius_,
        .maximum_temperature_millicelsius =
            maximum_temperature_millicelsius_,
        .temperature_sum_millicelsius = temperature_sum_millicelsius_,
        .minimum_supply_millivolts = minimum_supply_millivolts_,
        .maximum_supply_millivolts = maximum_supply_millivolts_,
        .supply_sum_millivolts = supply_sum_millivolts_,
    };
}

}  // namespace telemetry::host
