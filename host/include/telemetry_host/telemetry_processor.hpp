#pragma once

#include "telemetry_host/telemetry_metrics.hpp"
#include "telemetry_protocol/protocol_types.hpp"

#include <safe_concurrent_buffer.hpp>

#include <cstdint>

namespace telemetry::host {

struct ConsumerResult {
    bool success{};
    std::uint64_t frames_consumed{};

    [[nodiscard]] bool operator==(const ConsumerResult&) const noexcept =
        default;
};

class TelemetryProcessor final {
public:
    explicit TelemetryProcessor(TelemetryMetrics& metrics) noexcept;

    void process(
        const telemetry::protocol::TelemetryFrame& frame) noexcept;

private:
    TelemetryMetrics& metrics_;
};

[[nodiscard]] ConsumerResult consume_queued_frames(
    elite::concurrency::SafeConcurrentBuffer& buffer,
    TelemetryProcessor& processor) noexcept;

}  // namespace telemetry::host
