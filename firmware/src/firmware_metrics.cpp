#include "telemetry_firmware/firmware_metrics.hpp"

namespace telemetry::firmware {

void FirmwareMetrics::record_sample_generated() noexcept {
    ++values_.samples_generated;
}

void FirmwareMetrics::record_frame_serialized() noexcept {
    ++values_.frames_serialized;
}

void FirmwareMetrics::record_frame_enqueued() noexcept {
    ++values_.frames_enqueued;
}

void FirmwareMetrics::record_frame_transmitted() noexcept {
    ++values_.frames_transmitted;
}

void FirmwareMetrics::record_queue_push_failure() noexcept {
    ++values_.queue_push_failures;
}

void FirmwareMetrics::record_queue_pop_failure() noexcept {
    ++values_.queue_pop_failures;
}

void FirmwareMetrics::record_end_marker_sent() noexcept {
    ++values_.end_markers_sent;
}

void FirmwareMetrics::record_end_marker_received() noexcept {
    ++values_.end_markers_received;
}

FirmwareMetricsSnapshot FirmwareMetrics::snapshot() const noexcept {
    return values_;
}

}  // namespace telemetry::firmware
