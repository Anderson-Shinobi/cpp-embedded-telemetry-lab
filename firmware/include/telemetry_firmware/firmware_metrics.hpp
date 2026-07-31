#pragma once

#include <cstdint>
#include <type_traits>

namespace telemetry::firmware {

struct FirmwareMetricsSnapshot {
    std::uint64_t samples_generated{};
    std::uint64_t frames_serialized{};
    std::uint64_t frames_enqueued{};
    std::uint64_t frames_transmitted{};
    std::uint64_t queue_push_failures{};
    std::uint64_t queue_pop_failures{};
    std::uint64_t end_markers_sent{};
    std::uint64_t end_markers_received{};
};

static_assert(std::is_trivially_copyable_v<FirmwareMetricsSnapshot>);

class FirmwareMetrics {
public:
    void record_sample_generated() noexcept;
    void record_frame_serialized() noexcept;
    void record_frame_enqueued() noexcept;
    void record_frame_transmitted() noexcept;
    void record_queue_push_failure() noexcept;
    void record_queue_pop_failure() noexcept;
    void record_end_marker_sent() noexcept;
    void record_end_marker_received() noexcept;

    [[nodiscard]] FirmwareMetricsSnapshot snapshot() const noexcept;

private:
    FirmwareMetricsSnapshot values_{};
};

}  // namespace telemetry::firmware
