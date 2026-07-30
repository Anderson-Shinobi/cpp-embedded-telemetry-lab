#include "telemetry_host/frame_ingestor.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace telemetry::host {

FrameIngestor::FrameIngestor(
    elite::concurrency::SafeConcurrentBuffer& buffer,
    TelemetryMetrics& metrics) noexcept
    : buffer_{buffer},
      metrics_{metrics} {}

IngestResult FrameIngestor::ingest(
    const std::span<const std::uint8_t> bytes) {
    metrics_.record_input();

    const telemetry::protocol::ParseResult parsed{
        telemetry::protocol::deserialize(bytes)};
    if (!parsed.has_value()) {
        metrics_.record_parse_rejected(parsed.error);
        return IngestResult{
            .status = IngestStatus::parse_rejected,
            .parse_error = parsed.error,
        };
    }

    elite::concurrency::SafeConcurrentBuffer::ValueType queued_bytes{};
    queued_bytes.reserve(bytes.size());
    for (const std::uint8_t byte : bytes) {
        queued_bytes.push_back(static_cast<std::byte>(byte));
    }

    if (!buffer_.push(std::move(queued_bytes))) {
        metrics_.record_queue_closed();
        return IngestResult{
            .status = IngestStatus::queue_closed,
            .parse_error = telemetry::protocol::ParseError::none,
        };
    }

    metrics_.record_accepted();
    return IngestResult{
        .status = IngestStatus::accepted,
        .parse_error = telemetry::protocol::ParseError::none,
    };
}

}  // namespace telemetry::host
