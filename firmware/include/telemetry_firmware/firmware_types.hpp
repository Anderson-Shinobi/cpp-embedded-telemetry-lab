#pragma once

#include "telemetry_protocol/serializer.hpp"

#include <cstdint>
#include <type_traits>

namespace telemetry::firmware {

enum class QueueItemKind : std::uint8_t {
    frame,
    end_of_stream,
};

struct TelemetryQueueItem {
    QueueItemKind kind{QueueItemKind::frame};
    telemetry::protocol::SerializedFrame bytes{};
};

static_assert(std::is_trivially_copyable_v<TelemetryQueueItem>);

}  // namespace telemetry::firmware
