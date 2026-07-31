#include "telemetry_firmware/telemetry_producer.hpp"

#include "telemetry_protocol/serializer.hpp"

namespace telemetry::firmware {

TelemetryProducer::TelemetryProducer(
    DeterministicSensorModel& sensor_model,
    TelemetryQueue& queue,
    FirmwareMetrics& metrics) noexcept
    : sensor_model_{sensor_model}, queue_{queue}, metrics_{metrics} {}

bool TelemetryProducer::run() noexcept {
    while (sensor_model_.has_next()) {
        const telemetry::protocol::TelemetryFrame frame{sensor_model_.next()};
        metrics_.record_sample_generated();

        const telemetry::protocol::SerializedFrame serialized{
            telemetry::protocol::serialize(frame)};
        metrics_.record_frame_serialized();

        if (!queue_.push_frame(serialized)) {
            metrics_.record_queue_push_failure();
            return false;
        }
        metrics_.record_frame_enqueued();
    }

    if (!queue_.push_end_of_stream()) {
        metrics_.record_queue_push_failure();
        return false;
    }
    metrics_.record_end_marker_sent();
    return true;
}

}  // namespace telemetry::firmware
