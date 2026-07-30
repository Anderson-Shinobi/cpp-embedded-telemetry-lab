#include "telemetry_host/frame_ingestor.hpp"
#include "telemetry_host/telemetry_metrics.hpp"
#include "telemetry_protocol/protocol_types.hpp"
#include "telemetry_protocol/serializer.hpp"

#include <safe_concurrent_buffer.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

namespace telemetry::host {
namespace {

[[nodiscard]] std::vector<std::uint8_t> valid_bytes() {
    const telemetry::protocol::TelemetryFrame frame{
        .header =
            {
                .version = telemetry::protocol::ProtocolVersion::v1,
                .message_type =
                    telemetry::protocol::MessageType::telemetry_sample,
                .payload_size =
                    telemetry::protocol::telemetry_payload_size,
                .sequence = 7U,
                .timestamp_microseconds = 42U,
            },
        .payload =
            {
                .temperature_millicelsius = -1'000,
                .pressure_pascal = 100'000U,
                .supply_millivolts = 3'300U,
                .status_flags = 0U,
            },
    };
    const auto serialized{telemetry::protocol::serialize(frame)};
    return {serialized.begin(), serialized.end()};
}

TEST(FrameIngestorTest, AcceptsValidFrame) {
    elite::concurrency::SafeConcurrentBuffer buffer{2U};
    TelemetryMetrics metrics{};
    FrameIngestor ingestor{buffer, metrics};

    const IngestResult result{ingestor.ingest(valid_bytes())};

    EXPECT_EQ(result.status, IngestStatus::accepted);
    EXPECT_EQ(result.parse_error, telemetry::protocol::ParseError::none);
    buffer.close();
}

TEST(FrameIngestorTest, RejectsInvalidMagic) {
    auto bytes{valid_bytes()};
    bytes[0U] = 0U;
    elite::concurrency::SafeConcurrentBuffer buffer{2U};
    TelemetryMetrics metrics{};
    FrameIngestor ingestor{buffer, metrics};

    const IngestResult result{ingestor.ingest(bytes)};

    EXPECT_EQ(result.status, IngestStatus::parse_rejected);
    EXPECT_EQ(result.parse_error, telemetry::protocol::ParseError::invalid_magic);
    buffer.close();
}

TEST(FrameIngestorTest, RejectsUnsupportedVersion) {
    auto bytes{valid_bytes()};
    bytes[telemetry::protocol::version_offset] = 2U;
    elite::concurrency::SafeConcurrentBuffer buffer{2U};
    TelemetryMetrics metrics{};
    FrameIngestor ingestor{buffer, metrics};

    const IngestResult result{ingestor.ingest(bytes)};

    EXPECT_EQ(
        result.parse_error,
        telemetry::protocol::ParseError::unsupported_version);
    buffer.close();
}

TEST(FrameIngestorTest, RejectsUnsupportedMessageType) {
    auto bytes{valid_bytes()};
    bytes[telemetry::protocol::message_type_offset] = 2U;
    elite::concurrency::SafeConcurrentBuffer buffer{2U};
    TelemetryMetrics metrics{};
    FrameIngestor ingestor{buffer, metrics};

    const IngestResult result{ingestor.ingest(bytes)};

    EXPECT_EQ(
        result.parse_error,
        telemetry::protocol::ParseError::unsupported_message_type);
    buffer.close();
}

TEST(FrameIngestorTest, RejectsInvalidPayloadSize) {
    auto bytes{valid_bytes()};
    bytes[telemetry::protocol::payload_size_offset + 1U] = 11U;
    elite::concurrency::SafeConcurrentBuffer buffer{2U};
    TelemetryMetrics metrics{};
    FrameIngestor ingestor{buffer, metrics};

    const IngestResult result{ingestor.ingest(bytes)};

    EXPECT_EQ(
        result.parse_error,
        telemetry::protocol::ParseError::invalid_payload_size);
    buffer.close();
}

TEST(FrameIngestorTest, RejectsChecksumMismatch) {
    auto bytes{valid_bytes()};
    bytes[telemetry::protocol::pressure_offset] ^= 1U;
    elite::concurrency::SafeConcurrentBuffer buffer{2U};
    TelemetryMetrics metrics{};
    FrameIngestor ingestor{buffer, metrics};

    const IngestResult result{ingestor.ingest(bytes)};

    EXPECT_EQ(
        result.parse_error,
        telemetry::protocol::ParseError::checksum_mismatch);
    buffer.close();
}

TEST(FrameIngestorTest, RejectsTruncatedFrame) {
    auto bytes{valid_bytes()};
    bytes.pop_back();
    elite::concurrency::SafeConcurrentBuffer buffer{2U};
    TelemetryMetrics metrics{};
    FrameIngestor ingestor{buffer, metrics};

    const IngestResult result{ingestor.ingest(bytes)};

    EXPECT_EQ(
        result.parse_error,
        telemetry::protocol::ParseError::invalid_frame_size);
    buffer.close();
}

TEST(FrameIngestorTest, RejectsOversizedFrame) {
    auto bytes{valid_bytes()};
    bytes.push_back(0U);
    elite::concurrency::SafeConcurrentBuffer buffer{2U};
    TelemetryMetrics metrics{};
    FrameIngestor ingestor{buffer, metrics};

    const IngestResult result{ingestor.ingest(bytes)};

    EXPECT_EQ(
        result.parse_error,
        telemetry::protocol::ParseError::invalid_frame_size);
    buffer.close();
}

TEST(FrameIngestorTest, RejectedFrameDoesNotEnterQueue) {
    auto bytes{valid_bytes()};
    bytes[0U] = 0U;
    elite::concurrency::SafeConcurrentBuffer buffer{2U};
    TelemetryMetrics metrics{};
    FrameIngestor ingestor{buffer, metrics};

    static_cast<void>(ingestor.ingest(bytes));
    buffer.close();

    EXPECT_FALSE(buffer.pop().has_value());
    EXPECT_EQ(buffer.pushedCount(), 0U);
}

TEST(FrameIngestorTest, AcceptedFrameEntersQueue) {
    elite::concurrency::SafeConcurrentBuffer buffer{2U};
    TelemetryMetrics metrics{};
    FrameIngestor ingestor{buffer, metrics};

    static_cast<void>(ingestor.ingest(valid_bytes()));
    buffer.close();
    const auto queued{buffer.pop()};

    ASSERT_TRUE(queued.has_value());
    EXPECT_EQ(queued->size(), telemetry::protocol::frame_size);
}

TEST(FrameIngestorTest, ClosedQueueProducesExplicitResult) {
    elite::concurrency::SafeConcurrentBuffer buffer{2U};
    buffer.close();
    TelemetryMetrics metrics{};
    FrameIngestor ingestor{buffer, metrics};

    const IngestResult result{ingestor.ingest(valid_bytes())};

    EXPECT_EQ(result.status, IngestStatus::queue_closed);
    EXPECT_EQ(result.parse_error, telemetry::protocol::ParseError::none);
}

TEST(FrameIngestorTest, PreservesSpecificParseError) {
    auto bytes{valid_bytes()};
    bytes[telemetry::protocol::crc_offset] ^= 1U;
    elite::concurrency::SafeConcurrentBuffer buffer{2U};
    TelemetryMetrics metrics{};
    FrameIngestor ingestor{buffer, metrics};

    const IngestResult result{ingestor.ingest(bytes)};

    EXPECT_EQ(
        result.parse_error,
        telemetry::protocol::ParseError::checksum_mismatch);
    buffer.close();
}

TEST(FrameIngestorTest, UpdatesInputCounter) {
    elite::concurrency::SafeConcurrentBuffer buffer{2U};
    TelemetryMetrics metrics{};
    FrameIngestor ingestor{buffer, metrics};

    static_cast<void>(ingestor.ingest(valid_bytes()));

    EXPECT_EQ(metrics.snapshot().inputs_received, 1U);
    buffer.close();
}

TEST(FrameIngestorTest, UpdatesAcceptedCounter) {
    elite::concurrency::SafeConcurrentBuffer buffer{2U};
    TelemetryMetrics metrics{};
    FrameIngestor ingestor{buffer, metrics};

    static_cast<void>(ingestor.ingest(valid_bytes()));

    EXPECT_EQ(metrics.snapshot().frames_accepted, 1U);
    EXPECT_EQ(metrics.snapshot().frames_rejected, 0U);
    buffer.close();
}

TEST(FrameIngestorTest, UpdatesRejectedCounter) {
    auto bytes{valid_bytes()};
    bytes.clear();
    elite::concurrency::SafeConcurrentBuffer buffer{2U};
    TelemetryMetrics metrics{};
    FrameIngestor ingestor{buffer, metrics};

    static_cast<void>(ingestor.ingest(bytes));

    const TelemetryMetricsSnapshot snapshot{metrics.snapshot()};
    EXPECT_EQ(snapshot.frames_rejected, 1U);
    EXPECT_EQ(snapshot.parse_errors.invalid_frame_size, 1U);
    buffer.close();
}

}  // namespace
}  // namespace telemetry::host
