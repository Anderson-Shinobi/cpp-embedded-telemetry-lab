#include "telemetry_firmware/telemetry_queue.hpp"

#include "telemetry_firmware/firmware_types.hpp"

#include <zephyr/kernel.h>

#include <cstddef>

namespace telemetry::firmware {
namespace {

K_MSGQ_DEFINE(
    firmware_message_queue,
    sizeof(TelemetryQueueItem),
    telemetry_queue_capacity,
    4);

}  // namespace

TelemetryQueue::TelemetryQueue() noexcept {
    k_msgq_purge(&firmware_message_queue);
}

bool TelemetryQueue::push_frame(
    const telemetry::protocol::SerializedFrame& frame) noexcept {
    const TelemetryQueueItem item{
        .kind = QueueItemKind::frame,
        .bytes = frame,
    };
    return k_msgq_put(&firmware_message_queue, &item, K_FOREVER) == 0;
}

bool TelemetryQueue::push_end_of_stream() noexcept {
    const TelemetryQueueItem item{
        .kind = QueueItemKind::end_of_stream,
        .bytes = {},
    };
    return k_msgq_put(&firmware_message_queue, &item, K_FOREVER) == 0;
}

bool TelemetryQueue::pop(TelemetryQueueItem& item) noexcept {
    return k_msgq_get(&firmware_message_queue, &item, K_FOREVER) == 0;
}

bool TelemetryQueue::empty() const noexcept {
    return k_msgq_num_used_get(&firmware_message_queue) == 0U;
}

}  // namespace telemetry::firmware
