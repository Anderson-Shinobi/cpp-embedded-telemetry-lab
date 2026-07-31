#include "telemetry_firmware/telemetry_transmitter.hpp"

#include "telemetry_firmware/firmware_types.hpp"
#include "telemetry_firmware/hex_formatter.hpp"

#include <zephyr/sys/printk.h>

#include <cstdint>

namespace telemetry::firmware {

TelemetryTransmitter::TelemetryTransmitter(
    TelemetryQueue& queue,
    FirmwareMetrics& metrics) noexcept
    : queue_{queue}, metrics_{metrics} {}

bool TelemetryTransmitter::run() noexcept {
    std::uint32_t transmission_index{0U};

    while (true) {
        TelemetryQueueItem item{};
        if (!queue_.pop(item)) {
            metrics_.record_queue_pop_failure();
            return false;
        }

        if (item.kind == QueueItemKind::end_of_stream) {
            metrics_.record_end_marker_received();
            return true;
        }

        ++transmission_index;
        const HexFormatter::Buffer hexadecimal{HexFormatter::format(item.bytes)};
        printk(
            "TLFRAME %04u %s\n",
            static_cast<unsigned int>(transmission_index),
            hexadecimal.data());
        metrics_.record_frame_transmitted();
    }
}

}  // namespace telemetry::firmware
