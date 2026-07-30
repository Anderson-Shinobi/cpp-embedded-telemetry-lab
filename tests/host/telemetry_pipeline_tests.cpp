#include "telemetry_host/frame_ingestor.hpp"
#include "telemetry_host/simulated_frame_source.hpp"
#include "telemetry_host/telemetry_metrics.hpp"
#include "telemetry_host/telemetry_processor.hpp"

#include <safe_concurrent_buffer.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <thread>

namespace telemetry::host {
namespace {

struct PipelineObservation {
    TelemetryMetricsSnapshot snapshot{};
    ConsumerResult consumer{};
    std::uint64_t pushed{};
    std::uint64_t popped{};
};

[[nodiscard]] PipelineObservation run_pipeline() {
    elite::concurrency::SafeConcurrentBuffer buffer{2U};
    TelemetryMetrics metrics{};
    FrameIngestor ingestor{buffer, metrics};
    TelemetryProcessor processor{metrics};
    ConsumerResult consumer_result{};
    std::jthread consumer{[&buffer, &processor, &consumer_result]() {
        consumer_result = consume_queued_frames(buffer, processor);
    }};

    const SimulatedFrameSource source{};
    for (const RawFrame& frame : source.generate()) {
        static_cast<void>(ingestor.ingest(frame));
    }
    buffer.close();
    consumer.join();

    return PipelineObservation{
        .snapshot = metrics.snapshot(),
        .consumer = consumer_result,
        .pushed = buffer.pushedCount(),
        .popped = buffer.poppedCount(),
    };
}

TEST(TelemetryPipelineTest, CompleteScenarioTerminatesSuccessfully) {
    const PipelineObservation observation{run_pipeline()};

    EXPECT_TRUE(observation.consumer.success);
}

TEST(TelemetryPipelineTest, ProcessesEveryAcceptedFrame) {
    const auto observation{run_pipeline()};

    EXPECT_EQ(observation.snapshot.frames_accepted, 8U);
    EXPECT_EQ(observation.snapshot.frames_processed, 8U);
}

TEST(TelemetryPipelineTest, DoesNotProcessInvalidFrames) {
    const auto observation{run_pipeline()};

    EXPECT_EQ(observation.snapshot.inputs_received, 15U);
    EXPECT_EQ(observation.snapshot.frames_rejected, 7U);
    EXPECT_EQ(observation.snapshot.frames_processed, 8U);
}

TEST(TelemetryPipelineTest, CloseAllowsCompleteDrain) {
    const auto observation{run_pipeline()};

    EXPECT_EQ(observation.pushed, 8U);
    EXPECT_EQ(observation.popped, observation.pushed);
}

TEST(TelemetryPipelineTest, AcceptedAndProcessedCountsMatch) {
    const auto snapshot{run_pipeline().snapshot};

    EXPECT_EQ(snapshot.frames_accepted, snapshot.frames_processed);
}

TEST(TelemetryPipelineTest, CompleteScenarioProducesExpectedMetrics) {
    const auto snapshot{run_pipeline().snapshot};

    EXPECT_EQ(snapshot.inputs_received, 15U);
    EXPECT_EQ(snapshot.frames_accepted, 8U);
    EXPECT_EQ(snapshot.frames_rejected, 7U);
    EXPECT_EQ(snapshot.parse_errors.invalid_frame_size, 1U);
    EXPECT_EQ(snapshot.parse_errors.invalid_magic, 1U);
    EXPECT_EQ(snapshot.parse_errors.unsupported_version, 1U);
    EXPECT_EQ(snapshot.parse_errors.unsupported_message_type, 1U);
    EXPECT_EQ(snapshot.parse_errors.invalid_payload_size, 1U);
    EXPECT_EQ(snapshot.parse_errors.checksum_mismatch, 2U);
}

TEST(TelemetryPipelineTest, RepeatedExecutionProducesSameSnapshot) {
    const auto first{run_pipeline().snapshot};
    const auto second{run_pipeline().snapshot};

    EXPECT_EQ(first, second);
}

TEST(TelemetryPipelineTest, ClosedEmptyQueueEndsConsumer) {
    elite::concurrency::SafeConcurrentBuffer buffer{1U};
    TelemetryMetrics metrics{};
    TelemetryProcessor processor{metrics};
    buffer.close();

    const ConsumerResult result{consume_queued_frames(buffer, processor)};

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.frames_consumed, 0U);
}

TEST(TelemetryPipelineTest, ConsumerThreadIsJoinedNotDetached) {
    elite::concurrency::SafeConcurrentBuffer buffer{1U};
    TelemetryMetrics metrics{};
    TelemetryProcessor processor{metrics};
    ConsumerResult result{};
    std::jthread consumer{[&buffer, &processor, &result]() {
        result = consume_queued_frames(buffer, processor);
    }};
    EXPECT_TRUE(consumer.joinable());

    buffer.close();
    consumer.join();

    EXPECT_FALSE(consumer.joinable());
    EXPECT_TRUE(result.success);
}

TEST(TelemetryPipelineTest, DetectsDuplicateSequence) {
    const auto snapshot{run_pipeline().snapshot};

    EXPECT_EQ(snapshot.duplicate_sequences, 1U);
}

TEST(TelemetryPipelineTest, DetectsMissingSequence) {
    const auto snapshot{run_pipeline().snapshot};

    EXPECT_EQ(snapshot.missing_sequences, 1U);
}

TEST(TelemetryPipelineTest, DetectsOutOfOrderSequence) {
    const auto snapshot{run_pipeline().snapshot};

    EXPECT_EQ(snapshot.out_of_order_sequences, 1U);
}

TEST(TelemetryPipelineTest, AggregatesWarnings) {
    const auto snapshot{run_pipeline().snapshot};

    EXPECT_EQ(snapshot.sensor_warnings, 3U);
    EXPECT_EQ(snapshot.voltage_warnings, 3U);
    EXPECT_EQ(snapshot.communication_warnings, 3U);
    EXPECT_EQ(snapshot.watchdog_events, 3U);
}

TEST(TelemetryPipelineTest, AggregatesTemperatures) {
    const auto snapshot{run_pipeline().snapshot};

    EXPECT_EQ(snapshot.minimum_temperature_millicelsius, -10'000);
    EXPECT_EQ(snapshot.maximum_temperature_millicelsius, 30'000);
    EXPECT_EQ(snapshot.temperature_sum_millicelsius, 65'000);
}

TEST(TelemetryPipelineTest, AggregatesSupplyVoltages) {
    const auto snapshot{run_pipeline().snapshot};

    EXPECT_EQ(snapshot.minimum_supply_millivolts, 2'900U);
    EXPECT_EQ(snapshot.maximum_supply_millivolts, 3'600U);
    EXPECT_EQ(snapshot.supply_sum_millivolts, 26'000U);
}

}  // namespace
}  // namespace telemetry::host
