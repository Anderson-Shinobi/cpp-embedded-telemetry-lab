#pragma once

#include "telemetry_firmware/deterministic_sensor_model.hpp"
#include "telemetry_firmware/firmware_metrics.hpp"
#include "telemetry_firmware/telemetry_queue.hpp"

namespace telemetry::firmware {

class TelemetryProducer {
public:
    TelemetryProducer(
        DeterministicSensorModel& sensor_model,
        TelemetryQueue& queue,
        FirmwareMetrics& metrics) noexcept;

    [[nodiscard]] bool run() noexcept;

private:
    DeterministicSensorModel& sensor_model_;
    TelemetryQueue& queue_;
    FirmwareMetrics& metrics_;
};

}  // namespace telemetry::firmware
