#pragma once

#include "telemetry_firmware/firmware_metrics.hpp"
#include "telemetry_firmware/telemetry_queue.hpp"

namespace telemetry::firmware {

class TelemetryTransmitter {
public:
    TelemetryTransmitter(
        TelemetryQueue& queue,
        FirmwareMetrics& metrics) noexcept;

    [[nodiscard]] bool run() noexcept;

private:
    TelemetryQueue& queue_;
    FirmwareMetrics& metrics_;
};

}  // namespace telemetry::firmware
