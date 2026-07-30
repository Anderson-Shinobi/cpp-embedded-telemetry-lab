#include "telemetry_host/telemetry_metrics.hpp"
#include "telemetry_protocol/protocol_types.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <type_traits>

namespace telemetry::host {
namespace {

[[nodiscard]] telemetry::protocol::TelemetryFrame make_frame(
    const std::uint32_t sequence,
    const std::int32_t temperature = 0,
    const std::uint16_t supply = 3'300U,
    const std::uint16_t flags = 0U) {
    return telemetry::protocol::TelemetryFrame{
        .header =
            {
                .version = telemetry::protocol::ProtocolVersion::v1,
                .message_type =
                    telemetry::protocol::MessageType::telemetry_sample,
                .payload_size =
                    telemetry::protocol::telemetry_payload_size,
                .sequence = sequence,
                .timestamp_microseconds = sequence,
            },
        .payload =
            {
                .temperature_millicelsius = temperature,
                .pressure_pascal = 100'000U,
                .supply_millivolts = supply,
                .status_flags = flags,
            },
    };
}

[[nodiscard]] std::uint16_t flag(
    const telemetry::protocol::StatusFlag value) {
    return static_cast<std::uint16_t>(value);
}

TEST(TelemetryMetricsTest, InitialSnapshotContainsZeros) {
    const TelemetryMetrics metrics{};

    const TelemetryMetricsSnapshot snapshot{metrics.snapshot()};

    EXPECT_EQ(snapshot.inputs_received, 0U);
    EXPECT_EQ(snapshot.frames_accepted, 0U);
    EXPECT_EQ(snapshot.frames_rejected, 0U);
    EXPECT_EQ(snapshot.frames_processed, 0U);
    EXPECT_EQ(snapshot.missing_sequences, 0U);
}

TEST(TelemetryMetricsTest, FirstFrameDefinesFirstAndLastSequence) {
    TelemetryMetrics metrics{};

    metrics.record_processed(make_frame(10U));

    const auto snapshot{metrics.snapshot()};
    EXPECT_EQ(snapshot.first_sequence, 10U);
    EXPECT_EQ(snapshot.last_sequence, 10U);
}

TEST(TelemetryMetricsTest, IncreasingSequenceUpdatesLastSequence) {
    TelemetryMetrics metrics{};
    metrics.record_processed(make_frame(10U));

    metrics.record_processed(make_frame(11U));

    EXPECT_EQ(metrics.snapshot().last_sequence, 11U);
}

TEST(TelemetryMetricsTest, SequenceGapRecordsExactMissingCount) {
    TelemetryMetrics metrics{};
    metrics.record_processed(make_frame(10U));

    metrics.record_processed(make_frame(15U));

    EXPECT_EQ(metrics.snapshot().missing_sequences, 4U);
}

TEST(TelemetryMetricsTest, DuplicateSequenceIsRecorded) {
    TelemetryMetrics metrics{};
    metrics.record_processed(make_frame(10U));

    metrics.record_processed(make_frame(10U));

    EXPECT_EQ(metrics.snapshot().duplicate_sequences, 1U);
}

TEST(TelemetryMetricsTest, RegressiveSequenceIsRecorded) {
    TelemetryMetrics metrics{};
    metrics.record_processed(make_frame(10U));
    metrics.record_processed(make_frame(12U));

    metrics.record_processed(make_frame(11U));

    EXPECT_EQ(metrics.snapshot().out_of_order_sequences, 1U);
}

TEST(TelemetryMetricsTest, RegressiveSequenceDoesNotReduceLastSequence) {
    TelemetryMetrics metrics{};
    metrics.record_processed(make_frame(10U));
    metrics.record_processed(make_frame(12U));

    metrics.record_processed(make_frame(11U));

    EXPECT_EQ(metrics.snapshot().last_sequence, 12U);
}

TEST(TelemetryMetricsTest, CountsSensorWarning) {
    TelemetryMetrics metrics{};

    metrics.record_processed(make_frame(
        1U, 0, 3'300U, flag(telemetry::protocol::StatusFlag::sensor_warning)));

    EXPECT_EQ(metrics.snapshot().sensor_warnings, 1U);
}

TEST(TelemetryMetricsTest, CountsVoltageWarning) {
    TelemetryMetrics metrics{};

    metrics.record_processed(make_frame(
        1U, 0, 3'300U, flag(telemetry::protocol::StatusFlag::voltage_warning)));

    EXPECT_EQ(metrics.snapshot().voltage_warnings, 1U);
}

TEST(TelemetryMetricsTest, CountsCommunicationWarning) {
    TelemetryMetrics metrics{};

    metrics.record_processed(make_frame(
        1U,
        0,
        3'300U,
        flag(telemetry::protocol::StatusFlag::communication_warning)));

    EXPECT_EQ(metrics.snapshot().communication_warnings, 1U);
}

TEST(TelemetryMetricsTest, CountsWatchdogEvent) {
    TelemetryMetrics metrics{};

    metrics.record_processed(make_frame(
        1U, 0, 3'300U, flag(telemetry::protocol::StatusFlag::watchdog_event)));

    EXPECT_EQ(metrics.snapshot().watchdog_events, 1U);
}

TEST(TelemetryMetricsTest, CountsMultipleFlagsOnSameFrame) {
    TelemetryMetrics metrics{};
    const std::uint16_t flags{
        static_cast<std::uint16_t>(
            flag(telemetry::protocol::StatusFlag::sensor_warning) |
            flag(telemetry::protocol::StatusFlag::voltage_warning) |
            flag(telemetry::protocol::StatusFlag::communication_warning) |
            flag(telemetry::protocol::StatusFlag::watchdog_event))};

    metrics.record_processed(make_frame(1U, 0, 3'300U, flags));

    const auto snapshot{metrics.snapshot()};
    EXPECT_EQ(snapshot.sensor_warnings, 1U);
    EXPECT_EQ(snapshot.voltage_warnings, 1U);
    EXPECT_EQ(snapshot.communication_warnings, 1U);
    EXPECT_EQ(snapshot.watchdog_events, 1U);
}

TEST(TelemetryMetricsTest, CalculatesMinimumTemperature) {
    TelemetryMetrics metrics{};
    metrics.record_processed(make_frame(1U, 1'000));

    metrics.record_processed(make_frame(2U, -2'000));

    EXPECT_EQ(metrics.snapshot().minimum_temperature_millicelsius, -2'000);
}

TEST(TelemetryMetricsTest, CalculatesMaximumTemperature) {
    TelemetryMetrics metrics{};
    metrics.record_processed(make_frame(1U, 1'000));

    metrics.record_processed(make_frame(2U, 2'000));

    EXPECT_EQ(metrics.snapshot().maximum_temperature_millicelsius, 2'000);
}

TEST(TelemetryMetricsTest, CalculatesTemperatureAverageWithoutTruncatingSum) {
    TelemetryMetrics metrics{};
    metrics.record_processed(make_frame(1U, 1'000));
    metrics.record_processed(make_frame(2U, 2'000));

    const auto snapshot{metrics.snapshot()};

    EXPECT_EQ(snapshot.temperature_sum_millicelsius, 3'000);
    EXPECT_EQ(
        snapshot.temperature_sum_millicelsius /
            static_cast<std::int64_t>(snapshot.frames_processed),
        1'500);
}

TEST(TelemetryMetricsTest, PreservesNegativeTemperature) {
    TelemetryMetrics metrics{};

    metrics.record_processed(make_frame(1U, -40'000));

    const auto snapshot{metrics.snapshot()};
    EXPECT_EQ(snapshot.minimum_temperature_millicelsius, -40'000);
    EXPECT_EQ(snapshot.temperature_sum_millicelsius, -40'000);
}

TEST(TelemetryMetricsTest, CalculatesMinimumSupplyVoltage) {
    TelemetryMetrics metrics{};
    metrics.record_processed(make_frame(1U, 0, 3'300U));

    metrics.record_processed(make_frame(2U, 0, 2'900U));

    EXPECT_EQ(metrics.snapshot().minimum_supply_millivolts, 2'900U);
}

TEST(TelemetryMetricsTest, CalculatesMaximumSupplyVoltage) {
    TelemetryMetrics metrics{};
    metrics.record_processed(make_frame(1U, 0, 3'300U));

    metrics.record_processed(make_frame(2U, 0, 3'600U));

    EXPECT_EQ(metrics.snapshot().maximum_supply_millivolts, 3'600U);
}

TEST(TelemetryMetricsTest, CalculatesSupplyAverageWithoutTruncatingSum) {
    TelemetryMetrics metrics{};
    metrics.record_processed(make_frame(1U, 0, 3'000U));
    metrics.record_processed(make_frame(2U, 0, 4'000U));

    const auto snapshot{metrics.snapshot()};

    EXPECT_EQ(snapshot.supply_sum_millivolts, 7'000U);
    EXPECT_EQ(snapshot.supply_sum_millivolts / snapshot.frames_processed, 3'500U);
}

TEST(TelemetryMetricsTest, CountsProcessedFrames) {
    TelemetryMetrics metrics{};
    metrics.record_processed(make_frame(1U));
    metrics.record_processed(make_frame(2U));

    EXPECT_EQ(metrics.snapshot().frames_processed, 2U);
}

TEST(TelemetryMetricsTest, SnapshotIsCopyable) {
    static_assert(std::is_copy_constructible_v<TelemetryMetricsSnapshot>);
    TelemetryMetrics metrics{};
    metrics.record_processed(make_frame(1U));

    const TelemetryMetricsSnapshot first{metrics.snapshot()};
    const TelemetryMetricsSnapshot second{first};

    EXPECT_EQ(second, first);
}

TEST(TelemetryMetricsTest, SnapshotDoesNotExposeMutableState) {
    static_assert(
        !std::is_reference_v<
            decltype(TelemetryMetricsSnapshot::first_sequence)>);
    TelemetryMetrics metrics{};
    metrics.record_processed(make_frame(1U));
    TelemetryMetricsSnapshot detached{metrics.snapshot()};

    detached.first_sequence = 99U;

    EXPECT_EQ(metrics.snapshot().first_sequence, 1U);
}

TEST(TelemetryMetricsTest, EmptyStateHasNoInvalidExtrema) {
    const TelemetryMetrics metrics{};

    const auto snapshot{metrics.snapshot()};

    EXPECT_FALSE(snapshot.minimum_temperature_millicelsius.has_value());
    EXPECT_FALSE(snapshot.maximum_temperature_millicelsius.has_value());
    EXPECT_FALSE(snapshot.minimum_supply_millivolts.has_value());
    EXPECT_FALSE(snapshot.maximum_supply_millivolts.has_value());
}

TEST(TelemetryMetricsTest, WideAccumulatorsAvoidSilentTruncation) {
    TelemetryMetrics metrics{};
    constexpr std::int32_t maximum_temperature{
        std::numeric_limits<std::int32_t>::max()};
    constexpr std::uint16_t maximum_supply{
        std::numeric_limits<std::uint16_t>::max()};
    metrics.record_processed(
        make_frame(1U, maximum_temperature, maximum_supply));
    metrics.record_processed(
        make_frame(2U, maximum_temperature, maximum_supply));

    const auto snapshot{metrics.snapshot()};

    EXPECT_EQ(snapshot.temperature_sum_millicelsius, 4'294'967'294LL);
    EXPECT_EQ(snapshot.supply_sum_millivolts, 131'070ULL);
}

}  // namespace
}  // namespace telemetry::host
