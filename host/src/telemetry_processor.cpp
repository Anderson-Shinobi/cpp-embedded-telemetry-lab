#include "telemetry_host/telemetry_processor.hpp"

#include "telemetry_protocol/deserializer.hpp"
#include "telemetry_protocol/protocol_types.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace telemetry::host {

TelemetryProcessor::TelemetryProcessor(TelemetryMetrics& metrics) noexcept
    : metrics_{metrics} {}

void TelemetryProcessor::process(
    const telemetry::protocol::TelemetryFrame& frame) noexcept {
    metrics_.record_processed(frame);
}

ConsumerResult consume_queued_frames(
    elite::concurrency::SafeConcurrentBuffer& buffer,
    TelemetryProcessor& processor) noexcept {
    std::uint64_t consumed{0U};

    try {
        while (true) {
            std::optional<elite::concurrency::SafeConcurrentBuffer::ValueType>
                queued{buffer.pop()};
            if (!queued.has_value()) {
                return ConsumerResult{
                    .success = true,
                    .frames_consumed = consumed,
                };
            }
            if (queued->size() != telemetry::protocol::frame_size) {
                return ConsumerResult{
                    .success = false,
                    .frames_consumed = consumed,
                };
            }

            std::array<std::uint8_t, telemetry::protocol::frame_size> bytes{};
            for (std::size_t index{0U}; index < bytes.size(); ++index) {
                bytes[index] =
                    std::to_integer<std::uint8_t>((*queued)[index]);
            }

            const telemetry::protocol::ParseResult parsed{
                telemetry::protocol::deserialize(bytes)};
            if (!parsed.has_value()) {
                return ConsumerResult{
                    .success = false,
                    .frames_consumed = consumed,
                };
            }

            processor.process(*parsed.frame);
            ++consumed;
        }
    } catch (...) {
        return ConsumerResult{
            .success = false,
            .frames_consumed = consumed,
        };
    }
}

}  // namespace telemetry::host
