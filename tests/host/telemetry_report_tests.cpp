#include "telemetry_host/telemetry_metrics.hpp"
#include "telemetry_host/telemetry_report.hpp"

#include <gtest/gtest.h>

#include <string>

namespace telemetry::host {
namespace {

[[nodiscard]] TelemetryMetricsSnapshot complete_snapshot() {
    return TelemetryMetricsSnapshot{
        .inputs_received = 15U,
        .frames_accepted = 8U,
        .frames_rejected = 7U,
        .queue_closed_rejections = 0U,
        .frames_processed = 8U,
        .parse_errors =
            {
                .invalid_frame_size = 1U,
                .invalid_magic = 1U,
                .unsupported_version = 1U,
                .unsupported_message_type = 1U,
                .invalid_payload_size = 1U,
                .checksum_mismatch = 2U,
            },
        .first_sequence = 100U,
        .last_sequence = 106U,
        .missing_sequences = 1U,
        .duplicate_sequences = 1U,
        .out_of_order_sequences = 1U,
        .sensor_warnings = 3U,
        .voltage_warnings = 3U,
        .communication_warnings = 3U,
        .watchdog_events = 3U,
        .minimum_temperature_millicelsius = -10'000,
        .maximum_temperature_millicelsius = 30'000,
        .temperature_sum_millicelsius = 65'000,
        .minimum_supply_millivolts = 2'900U,
        .maximum_supply_millivolts = 3'600U,
        .supply_sum_millivolts = 26'000U,
    };
}

TEST(TelemetryReportTest, ContainsTitle) {
    const std::string report{make_text_report(complete_snapshot())};

    EXPECT_NE(report.find("Telemetry Host Pipeline Report"), std::string::npos);
}

TEST(TelemetryReportTest, ContainsGeneralCounts) {
    const std::string report{make_text_report(complete_snapshot())};

    EXPECT_NE(report.find("Inputs received: 15"), std::string::npos);
    EXPECT_NE(report.find("Frames accepted: 8"), std::string::npos);
    EXPECT_NE(report.find("Frames rejected: 7"), std::string::npos);
    EXPECT_NE(report.find("Frames processed: 8"), std::string::npos);
}

TEST(TelemetryReportTest, ContainsEveryParseErrorType) {
    const std::string report{make_text_report(complete_snapshot())};

    EXPECT_NE(report.find("Invalid frame size: 1"), std::string::npos);
    EXPECT_NE(report.find("Invalid magic: 1"), std::string::npos);
    EXPECT_NE(report.find("Unsupported version: 1"), std::string::npos);
    EXPECT_NE(
        report.find("Unsupported message type: 1"), std::string::npos);
    EXPECT_NE(report.find("Invalid payload size: 1"), std::string::npos);
    EXPECT_NE(report.find("Checksum mismatch: 2"), std::string::npos);
}

TEST(TelemetryReportTest, ContainsSequenceAnalysis) {
    const std::string report{make_text_report(complete_snapshot())};

    EXPECT_NE(report.find("First sequence: 100"), std::string::npos);
    EXPECT_NE(report.find("Last sequence: 106"), std::string::npos);
    EXPECT_NE(report.find("Missing sequences: 1"), std::string::npos);
    EXPECT_NE(report.find("Duplicate sequences: 1"), std::string::npos);
    EXPECT_NE(report.find("Out-of-order sequences: 1"), std::string::npos);
}

TEST(TelemetryReportTest, ContainsWarningCounts) {
    const std::string report{make_text_report(complete_snapshot())};

    EXPECT_NE(report.find("Sensor: 3"), std::string::npos);
    EXPECT_NE(report.find("Voltage: 3"), std::string::npos);
    EXPECT_NE(report.find("Communication: 3"), std::string::npos);
    EXPECT_NE(report.find("Watchdog: 3"), std::string::npos);
}

TEST(TelemetryReportTest, ContainsTemperatureStatistics) {
    const std::string report{make_text_report(complete_snapshot())};

    EXPECT_NE(report.find("Minimum: -10000 millicelsius"), std::string::npos);
    EXPECT_NE(report.find("Maximum: 30000 millicelsius"), std::string::npos);
    EXPECT_NE(report.find("Average: 8125 millicelsius"), std::string::npos);
}

TEST(TelemetryReportTest, ContainsSupplyVoltageStatistics) {
    const std::string report{make_text_report(complete_snapshot())};

    EXPECT_NE(report.find("Minimum: 2900 millivolts"), std::string::npos);
    EXPECT_NE(report.find("Maximum: 3600 millivolts"), std::string::npos);
    EXPECT_NE(report.find("Average: 3250 millivolts"), std::string::npos);
}

TEST(TelemetryReportTest, IsDeterministic) {
    const auto snapshot{complete_snapshot()};

    EXPECT_EQ(make_text_report(snapshot), make_text_report(snapshot));
}

TEST(TelemetryReportTest, DoesNotContainMemoryAddresses) {
    const std::string report{make_text_report(complete_snapshot())};

    EXPECT_EQ(report.find("0x"), std::string::npos);
}

TEST(TelemetryReportTest, DoesNotContainLocalPaths) {
    const std::string report{make_text_report(complete_snapshot())};

    EXPECT_EQ(report.find("/home/"), std::string::npos);
    EXPECT_EQ(report.find("/tmp/"), std::string::npos);
}

TEST(TelemetryReportTest, DoesNotContainRealTimestamps) {
    const std::string report{make_text_report(complete_snapshot())};

    EXPECT_EQ(report.find("Timestamp"), std::string::npos);
    EXPECT_EQ(report.find("UTC"), std::string::npos);
}

TEST(TelemetryReportTest, DefinesEmptySnapshotOutput) {
    const std::string report{make_text_report(TelemetryMetricsSnapshot{})};

    EXPECT_NE(report.find("First sequence: N/A"), std::string::npos);
    EXPECT_NE(report.find("Minimum: N/A millicelsius"), std::string::npos);
    EXPECT_NE(report.find("Average: N/A millivolts"), std::string::npos);
}

TEST(TelemetryReportTest, SameSnapshotIsByteForByteIdentical) {
    const auto snapshot{complete_snapshot()};
    const std::string first{make_text_report(snapshot)};
    const std::string second{make_text_report(snapshot)};

    EXPECT_EQ(first.size(), second.size());
    EXPECT_EQ(first, second);
}

}  // namespace
}  // namespace telemetry::host
