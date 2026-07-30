#pragma once

#include "telemetry_host/telemetry_metrics.hpp"
#include "telemetry_protocol/deserializer.hpp"

#include <safe_concurrent_buffer.hpp>

#include <cstdint>
#include <span>

namespace telemetry::host {

enum class IngestStatus {
    accepted,
    parse_rejected,
    queue_closed,
};

struct IngestResult {
    IngestStatus status{IngestStatus::parse_rejected};
    telemetry::protocol::ParseError parse_error{
        telemetry::protocol::ParseError::none};

    [[nodiscard]] bool operator==(const IngestResult&) const noexcept = default;
};

class FrameIngestor final {
public:
    FrameIngestor(
        elite::concurrency::SafeConcurrentBuffer& buffer,
        TelemetryMetrics& metrics) noexcept;

    [[nodiscard]] IngestResult ingest(
        std::span<const std::uint8_t> bytes);

private:
    elite::concurrency::SafeConcurrentBuffer& buffer_;
    TelemetryMetrics& metrics_;
};

}  // namespace telemetry::host
