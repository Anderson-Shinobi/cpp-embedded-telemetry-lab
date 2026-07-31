#pragma once

#include "telemetry_firmware/firmware_types.hpp"
#include "telemetry_protocol/serializer.hpp"

#include <cstddef>

namespace telemetry::firmware {

inline constexpr std::size_t telemetry_queue_capacity{4U};

class TelemetryQueue {
public:
    TelemetryQueue() noexcept;

    TelemetryQueue(const TelemetryQueue&) = delete;
    TelemetryQueue& operator=(const TelemetryQueue&) = delete;

    [[nodiscard]] bool push_frame(
        const telemetry::protocol::SerializedFrame& frame) noexcept;
    [[nodiscard]] bool push_end_of_stream() noexcept;
    [[nodiscard]] bool pop(TelemetryQueueItem& item) noexcept;
    [[nodiscard]] bool empty() const noexcept;
};

}  // namespace telemetry::firmware
