#include "telemetry_host/telemetry_report.hpp"

#include <cstdint>
#include <optional>
#include <sstream>
#include <string>

namespace telemetry::host {
namespace {

template <typename Integer>
[[nodiscard]] std::string format_optional(
    const std::optional<Integer>& value) {
    if (!value.has_value()) {
        return "N/A";
    }
    return std::to_string(*value);
}

[[nodiscard]] std::string format_signed_average(
    const std::int64_t sum,
    const std::uint64_t count) {
    if (count == 0U) {
        return "N/A";
    }

    const bool negative{sum < 0};
    const std::uint64_t magnitude{
        negative
            ? static_cast<std::uint64_t>(-(sum + 1)) + 1U
            : static_cast<std::uint64_t>(sum)};
    const std::uint64_t quotient{magnitude / count};
    const std::uint64_t remainder{magnitude % count};
    const std::string sign{negative ? "-" : ""};
    if (remainder == 0U) {
        return sign + std::to_string(quotient);
    }
    return sign + std::to_string(magnitude) + "/" + std::to_string(count);
}

[[nodiscard]] std::string format_unsigned_average(
    const std::uint64_t sum,
    const std::uint64_t count) {
    if (count == 0U) {
        return "N/A";
    }

    const std::uint64_t quotient{sum / count};
    const std::uint64_t remainder{sum % count};
    if (remainder == 0U) {
        return std::to_string(quotient);
    }
    return std::to_string(sum) + "/" + std::to_string(count);
}

}  // namespace

std::string make_text_report(const TelemetryMetricsSnapshot& snapshot) {
    std::ostringstream report{};
    report << "Telemetry Host Pipeline Report\n"
           << "==============================\n"
           << "Inputs received: " << snapshot.inputs_received << '\n'
           << "Frames accepted: " << snapshot.frames_accepted << '\n'
           << "Frames rejected: " << snapshot.frames_rejected << '\n'
           << "Frames processed: " << snapshot.frames_processed << '\n'
           << "Queue closed rejections: "
           << snapshot.queue_closed_rejections << '\n'
           << "Parse errors:\n"
           << "  Invalid frame size: "
           << snapshot.parse_errors.invalid_frame_size << '\n'
           << "  Invalid magic: " << snapshot.parse_errors.invalid_magic << '\n'
           << "  Unsupported version: "
           << snapshot.parse_errors.unsupported_version << '\n'
           << "  Unsupported message type: "
           << snapshot.parse_errors.unsupported_message_type << '\n'
           << "  Invalid payload size: "
           << snapshot.parse_errors.invalid_payload_size << '\n'
           << "  Checksum mismatch: "
           << snapshot.parse_errors.checksum_mismatch << '\n'
           << "Sequence analysis:\n"
           << "  First sequence: "
           << format_optional(snapshot.first_sequence) << '\n'
           << "  Last sequence: "
           << format_optional(snapshot.last_sequence) << '\n'
           << "  Missing sequences: " << snapshot.missing_sequences << '\n'
           << "  Duplicate sequences: " << snapshot.duplicate_sequences
           << '\n'
           << "  Out-of-order sequences: "
           << snapshot.out_of_order_sequences << '\n'
           << "Warnings:\n"
           << "  Sensor: " << snapshot.sensor_warnings << '\n'
           << "  Voltage: " << snapshot.voltage_warnings << '\n'
           << "  Communication: " << snapshot.communication_warnings << '\n'
           << "  Watchdog: " << snapshot.watchdog_events << '\n'
           << "Temperature:\n"
           << "  Minimum: "
           << format_optional(snapshot.minimum_temperature_millicelsius)
           << " millicelsius\n"
           << "  Maximum: "
           << format_optional(snapshot.maximum_temperature_millicelsius)
           << " millicelsius\n"
           << "  Average: "
           << format_signed_average(
                  snapshot.temperature_sum_millicelsius,
                  snapshot.frames_processed)
           << " millicelsius\n"
           << "Supply voltage:\n"
           << "  Minimum: "
           << format_optional(snapshot.minimum_supply_millivolts)
           << " millivolts\n"
           << "  Maximum: "
           << format_optional(snapshot.maximum_supply_millivolts)
           << " millivolts\n"
           << "  Average: "
           << format_unsigned_average(
                  snapshot.supply_sum_millivolts,
                  snapshot.frames_processed)
           << " millivolts\n";
    return report.str();
}

}  // namespace telemetry::host
