#include "telemetry_firmware/deterministic_sensor_model.hpp"
#include "telemetry_firmware/firmware_metrics.hpp"
#include "telemetry_firmware/firmware_types.hpp"
#include "telemetry_firmware/hex_formatter.hpp"
#include "telemetry_firmware/telemetry_producer.hpp"
#include "telemetry_firmware/telemetry_queue.hpp"
#include "telemetry_firmware/telemetry_transmitter.hpp"
#include "telemetry_protocol/deserializer.hpp"
#include "telemetry_protocol/protocol_types.hpp"
#include "telemetry_protocol/serializer.hpp"

#include <zephyr/kernel.h>
#include <zephyr/ztest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace {

using telemetry::firmware::DeterministicSensorModel;
using telemetry::firmware::FirmwareMetrics;
using telemetry::firmware::FirmwareMetricsSnapshot;
using telemetry::firmware::HexFormatter;
using telemetry::firmware::QueueItemKind;
using telemetry::firmware::TelemetryProducer;
using telemetry::firmware::TelemetryQueue;
using telemetry::firmware::TelemetryQueueItem;
using telemetry::firmware::TelemetryTransmitter;
using telemetry::protocol::SerializedFrame;
using telemetry::protocol::StatusFlag;
using telemetry::protocol::TelemetryFrame;

constexpr std::size_t test_thread_stack_size{4'096U};
constexpr int test_producer_priority{5};
constexpr int test_consumer_priority{4};

K_THREAD_STACK_DEFINE(test_producer_stack, test_thread_stack_size);
K_THREAD_STACK_DEFINE(test_consumer_stack, test_thread_stack_size);
struct k_thread test_producer_thread;
struct k_thread test_consumer_thread;

struct ProducerContext {
    TelemetryProducer* producer{};
    bool succeeded{};
};

struct TransmitterContext {
    TelemetryTransmitter* transmitter{};
    bool succeeded{};
};

struct DrainContext {
    TelemetryQueue* queue{};
    FirmwareMetrics* metrics{};
    std::array<SerializedFrame, telemetry::firmware::deterministic_sample_count>
        frames{};
    std::size_t frame_count{};
    std::size_t end_marker_count{};
    bool succeeded{true};
};

struct ScenarioResult {
    FirmwareMetricsSnapshot metrics{};
    bool producer_succeeded{};
    bool consumer_succeeded{};
    bool queue_empty{};
};

struct DrainScenarioResult {
    FirmwareMetricsSnapshot metrics{};
    std::array<SerializedFrame, telemetry::firmware::deterministic_sample_count>
        frames{};
    std::size_t frame_count{};
    std::size_t end_marker_count{};
    bool producer_succeeded{};
    bool consumer_succeeded{};
    bool queue_empty{};
};

void producer_entry(void* first, void*, void*) {
    auto* const context{static_cast<ProducerContext*>(first)};
    context->succeeded = context->producer->run();
}

void transmitter_entry(void* first, void*, void*) {
    auto* const context{static_cast<TransmitterContext*>(first)};
    context->succeeded = context->transmitter->run();
}

void drain_entry(void* first, void*, void*) {
    auto* const context{static_cast<DrainContext*>(first)};

    while (true) {
        TelemetryQueueItem item{};
        if (!context->queue->pop(item)) {
            context->metrics->record_queue_pop_failure();
            context->succeeded = false;
            return;
        }

        if (item.kind == QueueItemKind::end_of_stream) {
            ++context->end_marker_count;
            context->metrics->record_end_marker_received();
            return;
        }

        if (context->frame_count >= context->frames.size()) {
            context->succeeded = false;
            return;
        }

        context->frames[context->frame_count] = item.bytes;
        ++context->frame_count;
        context->metrics->record_frame_transmitted();
    }
}

ScenarioResult run_transmitter_scenario() {
    DeterministicSensorModel model{};
    TelemetryQueue queue{};
    FirmwareMetrics metrics{};
    TelemetryProducer producer{model, queue, metrics};
    TelemetryTransmitter transmitter{queue, metrics};
    ProducerContext producer_context{.producer = &producer};
    TransmitterContext transmitter_context{.transmitter = &transmitter};

    k_tid_t consumer_id{k_thread_create(
        &test_consumer_thread,
        test_consumer_stack,
        K_THREAD_STACK_SIZEOF(test_consumer_stack),
        transmitter_entry,
        &transmitter_context,
        nullptr,
        nullptr,
        test_consumer_priority,
        0,
        K_NO_WAIT)};
    k_tid_t producer_id{k_thread_create(
        &test_producer_thread,
        test_producer_stack,
        K_THREAD_STACK_SIZEOF(test_producer_stack),
        producer_entry,
        &producer_context,
        nullptr,
        nullptr,
        test_producer_priority,
        0,
        K_NO_WAIT)};

    zassert_equal(k_thread_join(producer_id, K_FOREVER), 0);
    zassert_equal(k_thread_join(consumer_id, K_FOREVER), 0);

    return ScenarioResult{
        .metrics = metrics.snapshot(),
        .producer_succeeded = producer_context.succeeded,
        .consumer_succeeded = transmitter_context.succeeded,
        .queue_empty = queue.empty(),
    };
}

DrainScenarioResult run_drain_scenario() {
    DeterministicSensorModel model{};
    TelemetryQueue queue{};
    FirmwareMetrics metrics{};
    TelemetryProducer producer{model, queue, metrics};
    ProducerContext producer_context{.producer = &producer};
    DrainContext drain_context{.queue = &queue, .metrics = &metrics};

    k_tid_t consumer_id{k_thread_create(
        &test_consumer_thread,
        test_consumer_stack,
        K_THREAD_STACK_SIZEOF(test_consumer_stack),
        drain_entry,
        &drain_context,
        nullptr,
        nullptr,
        test_consumer_priority,
        0,
        K_NO_WAIT)};
    k_tid_t producer_id{k_thread_create(
        &test_producer_thread,
        test_producer_stack,
        K_THREAD_STACK_SIZEOF(test_producer_stack),
        producer_entry,
        &producer_context,
        nullptr,
        nullptr,
        test_producer_priority,
        0,
        K_NO_WAIT)};

    zassert_equal(k_thread_join(producer_id, K_FOREVER), 0);
    zassert_equal(k_thread_join(consumer_id, K_FOREVER), 0);

    return DrainScenarioResult{
        .metrics = metrics.snapshot(),
        .frames = drain_context.frames,
        .frame_count = drain_context.frame_count,
        .end_marker_count = drain_context.end_marker_count,
        .producer_succeeded = producer_context.succeeded,
        .consumer_succeeded = drain_context.succeeded,
        .queue_empty = queue.empty(),
    };
}

std::array<TelemetryFrame, telemetry::firmware::deterministic_sample_count>
collect_frames() {
    DeterministicSensorModel model{};
    std::array<TelemetryFrame, telemetry::firmware::deterministic_sample_count>
        frames{};
    for (std::size_t index{0U}; index < frames.size(); ++index) {
        frames[index] = model.next();
    }
    return frames;
}

bool has_flag(const TelemetryFrame& frame, const StatusFlag flag) {
    const std::uint16_t mask{static_cast<std::uint16_t>(flag)};
    return (frame.payload.status_flags & mask) != 0U;
}

}  // namespace

ZTEST(firmware_telemetry, test_sensor_model_starts_with_eight_samples) {
    DeterministicSensorModel model{};
    zassert_equal(model.remaining(), 8U);
}

ZTEST(firmware_telemetry, test_remaining_is_initially_eight) {
    DeterministicSensorModel model{};
    zassert_equal(
        model.remaining(), telemetry::firmware::deterministic_sample_count);
}

ZTEST(firmware_telemetry, test_has_next_is_initially_true) {
    DeterministicSensorModel model{};
    zassert_true(model.has_next());
}

ZTEST(firmware_telemetry, test_has_next_is_false_after_eight_reads) {
    DeterministicSensorModel model{};
    for (std::size_t index{0U};
         index < telemetry::firmware::deterministic_sample_count;
         ++index) {
        (void)model.next();
    }
    zassert_false(model.has_next());
}

ZTEST(firmware_telemetry, test_first_sequence_is_1000) {
    DeterministicSensorModel model{};
    zassert_equal(model.next().header.sequence, 1000U);
}

ZTEST(firmware_telemetry, test_last_sequence_is_1007) {
    const auto frames{collect_frames()};
    zassert_equal(frames.back().header.sequence, 1007U);
}

ZTEST(firmware_telemetry, test_sequences_are_continuous) {
    const auto frames{collect_frames()};
    for (std::size_t index{1U}; index < frames.size(); ++index) {
        zassert_equal(
            frames[index].header.sequence,
            frames[index - 1U].header.sequence + 1U);
    }
}

ZTEST(firmware_telemetry, test_timestamps_are_increasing) {
    const auto frames{collect_frames()};
    for (std::size_t index{1U}; index < frames.size(); ++index) {
        zassert_true(
            frames[index].header.timestamp_microseconds >
            frames[index - 1U].header.timestamp_microseconds);
    }
}

ZTEST(firmware_telemetry, test_timestamp_increment_is_constant) {
    const auto frames{collect_frames()};
    for (std::size_t index{1U}; index < frames.size(); ++index) {
        zassert_equal(
            frames[index].header.timestamp_microseconds -
                frames[index - 1U].header.timestamp_microseconds,
            telemetry::firmware::timestamp_increment_microseconds);
    }
}

ZTEST(firmware_telemetry, test_negative_temperature_is_present) {
    const auto frames{collect_frames()};
    bool found{false};
    for (const auto& frame : frames) {
        found = found || frame.payload.temperature_millicelsius < 0;
    }
    zassert_true(found);
}

ZTEST(firmware_telemetry, test_sample_without_warning_is_present) {
    const auto frames{collect_frames()};
    bool found{false};
    for (const auto& frame : frames) {
        found = found || frame.payload.status_flags == 0U;
    }
    zassert_true(found);
}

ZTEST(firmware_telemetry, test_sensor_warning_is_present) {
    const auto frames{collect_frames()};
    bool found{false};
    for (const auto& frame : frames) {
        found = found || has_flag(frame, StatusFlag::sensor_warning);
    }
    zassert_true(found);
}

ZTEST(firmware_telemetry, test_voltage_warning_is_present) {
    const auto frames{collect_frames()};
    bool found{false};
    for (const auto& frame : frames) {
        found = found || has_flag(frame, StatusFlag::voltage_warning);
    }
    zassert_true(found);
}

ZTEST(firmware_telemetry, test_communication_warning_is_present) {
    const auto frames{collect_frames()};
    bool found{false};
    for (const auto& frame : frames) {
        found = found || has_flag(frame, StatusFlag::communication_warning);
    }
    zassert_true(found);
}

ZTEST(firmware_telemetry, test_watchdog_event_is_present) {
    const auto frames{collect_frames()};
    bool found{false};
    for (const auto& frame : frames) {
        found = found || has_flag(frame, StatusFlag::watchdog_event);
    }
    zassert_true(found);
}

ZTEST(firmware_telemetry, test_multiple_flags_are_present) {
    const auto frames{collect_frames()};
    bool found{false};
    for (const auto& frame : frames) {
        const std::uint16_t flags{frame.payload.status_flags};
        found = found || (flags != 0U && (flags & (flags - 1U)) != 0U);
    }
    zassert_true(found);
}

ZTEST(firmware_telemetry, test_serialized_frame_has_34_bytes) {
    zassert_equal(telemetry::protocol::frame_size, 34U);
}

ZTEST(firmware_telemetry, test_serialized_frame_has_tl_magic) {
    DeterministicSensorModel model{};
    const auto bytes{telemetry::protocol::serialize(model.next())};
    zassert_equal(bytes[0U], 0x54U);
    zassert_equal(bytes[1U], 0x4CU);
}

ZTEST(firmware_telemetry, test_serialized_frame_has_supported_version) {
    DeterministicSensorModel model{};
    const auto bytes{telemetry::protocol::serialize(model.next())};
    zassert_equal(bytes[telemetry::protocol::version_offset], 1U);
}

ZTEST(firmware_telemetry, test_serialized_frame_has_telemetry_type) {
    DeterministicSensorModel model{};
    const auto bytes{telemetry::protocol::serialize(model.next())};
    zassert_equal(bytes[telemetry::protocol::message_type_offset], 1U);
}

ZTEST(firmware_telemetry, test_serialized_frame_has_payload_size_12) {
    DeterministicSensorModel model{};
    const auto bytes{telemetry::protocol::serialize(model.next())};
    zassert_equal(bytes[telemetry::protocol::payload_size_offset], 0U);
    zassert_equal(bytes[telemetry::protocol::payload_size_offset + 1U], 12U);
}

ZTEST(firmware_telemetry, test_serialized_frame_crc_is_valid) {
    DeterministicSensorModel model{};
    const auto bytes{telemetry::protocol::serialize(model.next())};
    zassert_true(telemetry::protocol::deserialize(bytes).has_value());
}

ZTEST(firmware_telemetry, test_deserialize_accepts_every_frame) {
    const auto frames{collect_frames()};
    for (const auto& frame : frames) {
        const auto bytes{telemetry::protocol::serialize(frame)};
        zassert_true(telemetry::protocol::deserialize(bytes).has_value());
    }
}

ZTEST(firmware_telemetry, test_round_trip_preserves_every_sample) {
    const auto frames{collect_frames()};
    for (const auto& frame : frames) {
        const auto result{
            telemetry::protocol::deserialize(
                telemetry::protocol::serialize(frame))};
        zassert_true(result.has_value());
        zassert_equal(result.frame.value(), frame);
    }
}

ZTEST(firmware_telemetry, test_queue_item_is_trivially_copyable) {
    zassert_true(std::is_trivially_copyable_v<TelemetryQueueItem>);
}

ZTEST(firmware_telemetry, test_frame_and_end_marker_are_distinct) {
    zassert_not_equal(
        static_cast<std::uint8_t>(QueueItemKind::frame),
        static_cast<std::uint8_t>(QueueItemKind::end_of_stream));
}

ZTEST(firmware_telemetry, test_formatter_produces_68_characters) {
    const SerializedFrame bytes{};
    const auto output{HexFormatter::format(bytes)};
    zassert_equal(telemetry::firmware::hexadecimal_frame_length, 68U);
    zassert_equal(output[68U], '\0');
}

ZTEST(firmware_telemetry, test_formatter_uses_uppercase_hexadecimal) {
    SerializedFrame bytes{};
    bytes[0U] = 0xABU;
    bytes[1U] = 0xCDU;
    bytes[2U] = 0xEFU;
    const auto output{HexFormatter::format(bytes)};
    zassert_mem_equal(output.data(), "ABCDEF", 6U);
}

ZTEST(firmware_telemetry, test_formatter_preserves_leading_zero) {
    SerializedFrame bytes{};
    bytes[0U] = 0x01U;
    const auto output{HexFormatter::format(bytes)};
    zassert_equal(output[0U], '0');
    zassert_equal(output[1U], '1');
}

ZTEST(firmware_telemetry, test_formatter_handles_zero_byte) {
    const SerializedFrame bytes{};
    const auto output{HexFormatter::format(bytes)};
    for (std::size_t index{0U};
         index < telemetry::firmware::hexadecimal_frame_length;
         ++index) {
        zassert_equal(output[index], '0');
    }
}

ZTEST(firmware_telemetry, test_formatter_handles_ff_byte) {
    SerializedFrame bytes{};
    bytes.fill(0xFFU);
    const auto output{HexFormatter::format(bytes)};
    for (std::size_t index{0U};
         index < telemetry::firmware::hexadecimal_frame_length;
         ++index) {
        zassert_equal(output[index], 'F');
    }
}

ZTEST(firmware_telemetry, test_formatter_is_deterministic) {
    DeterministicSensorModel model{};
    const auto bytes{telemetry::protocol::serialize(model.next())};
    zassert_equal(HexFormatter::format(bytes), HexFormatter::format(bytes));
}

ZTEST(firmware_telemetry, test_metrics_start_at_zero) {
    const FirmwareMetrics metrics{};
    const auto snapshot{metrics.snapshot()};
    zassert_equal(snapshot.samples_generated, 0U);
    zassert_equal(snapshot.frames_serialized, 0U);
    zassert_equal(snapshot.frames_enqueued, 0U);
    zassert_equal(snapshot.frames_transmitted, 0U);
    zassert_equal(snapshot.queue_push_failures, 0U);
    zassert_equal(snapshot.queue_pop_failures, 0U);
    zassert_equal(snapshot.end_markers_sent, 0U);
    zassert_equal(snapshot.end_markers_received, 0U);
}

ZTEST(firmware_telemetry, test_metrics_snapshot_is_copyable) {
    zassert_true(std::is_copy_constructible_v<FirmwareMetricsSnapshot>);
    zassert_true(std::is_copy_assignable_v<FirmwareMetricsSnapshot>);
}

ZTEST(firmware_telemetry, test_producer_generates_eight_frames) {
    const auto scenario{run_drain_scenario()};
    zassert_equal(scenario.metrics.samples_generated, 8U);
    zassert_equal(scenario.metrics.frames_serialized, 8U);
    zassert_equal(scenario.metrics.frames_enqueued, 8U);
}

ZTEST(firmware_telemetry, test_transmitter_receives_eight_frames) {
    const auto scenario{run_transmitter_scenario()};
    zassert_equal(scenario.metrics.frames_transmitted, 8U);
}

ZTEST(firmware_telemetry, test_producer_sends_one_end_marker) {
    const auto scenario{run_drain_scenario()};
    zassert_equal(scenario.metrics.end_markers_sent, 1U);
}

ZTEST(firmware_telemetry, test_consumer_receives_one_end_marker) {
    const auto scenario{run_drain_scenario()};
    zassert_equal(scenario.end_marker_count, 1U);
    zassert_equal(scenario.metrics.end_markers_received, 1U);
}

ZTEST(firmware_telemetry, test_normal_scenario_has_no_queue_failures) {
    const auto scenario{run_transmitter_scenario()};
    zassert_equal(scenario.metrics.queue_push_failures, 0U);
    zassert_equal(scenario.metrics.queue_pop_failures, 0U);
}

ZTEST(firmware_telemetry, test_frame_order_is_preserved) {
    const auto scenario{run_drain_scenario()};
    zassert_equal(scenario.frame_count, 8U);
    for (std::size_t index{0U}; index < scenario.frame_count; ++index) {
        const auto result{
            telemetry::protocol::deserialize(scenario.frames[index])};
        zassert_true(result.has_value());
        zassert_equal(
            result.frame->header.sequence,
            telemetry::firmware::initial_sequence +
                static_cast<std::uint32_t>(index));
    }
}

ZTEST(firmware_telemetry, test_no_frame_remains_after_end_marker) {
    const auto scenario{run_drain_scenario()};
    zassert_true(scenario.queue_empty);
    zassert_equal(scenario.frame_count, 8U);
    zassert_equal(scenario.end_marker_count, 1U);
}

ZTEST(firmware_telemetry, test_complete_scenario_terminates) {
    const auto scenario{run_transmitter_scenario()};
    zassert_true(scenario.producer_succeeded);
    zassert_true(scenario.consumer_succeeded);
    zassert_true(scenario.queue_empty);
}

ZTEST(firmware_telemetry, test_repeated_scenarios_have_identical_logical_output) {
    const auto first{run_drain_scenario()};
    const auto second{run_drain_scenario()};
    zassert_equal(first.frame_count, second.frame_count);
    zassert_equal(first.frames, second.frames);
    zassert_equal(first.end_marker_count, second.end_marker_count);
}

ZTEST_SUITE(firmware_telemetry, nullptr, nullptr, nullptr, nullptr, nullptr);
