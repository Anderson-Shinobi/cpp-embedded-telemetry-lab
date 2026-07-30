#include "telemetry_protocol/crc32.hpp"
#include "telemetry_protocol/deserializer.hpp"
#include "telemetry_protocol/protocol_types.hpp"
#include "telemetry_protocol/serializer.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <limits>
#include <span>

namespace telemetry::protocol {
namespace {

TelemetryFrame make_frame() {
    return TelemetryFrame{
        .header =
            {
                .version = ProtocolVersion::v1,
                .message_type = MessageType::telemetry_sample,
                .payload_size = telemetry_payload_size,
                .sequence = 0x12345678U,
                .timestamp_microseconds = 0x0102030405060708ULL,
            },
        .payload =
            {
                .temperature_millicelsius = 25'125,
                .pressure_pascal = 101'325U,
                .supply_millivolts = 3'300U,
                .status_flags =
                    static_cast<std::uint16_t>(StatusFlag::sensor_warning) |
                    static_cast<std::uint16_t>(StatusFlag::watchdog_event),
            },
    };
}

ParseResult round_trip(const TelemetryFrame& frame) {
    const SerializedFrame bytes{serialize(frame)};
    return deserialize(bytes);
}

TEST(Crc32Test, MatchesStandardCheckVector) {
    constexpr std::array<std::uint8_t, 9U> input{
        '1', '2', '3', '4', '5', '6', '7', '8', '9'};

    EXPECT_EQ(calculate_crc32(input), 0xCBF43926U);
}

TEST(SerializationTest, ProducesExactlyThirtyFourBytes) {
    const SerializedFrame bytes{serialize(make_frame())};

    EXPECT_EQ(bytes.size(), 34U);
    EXPECT_EQ(bytes.size(), frame_size);
}

TEST(SerializationTest, WritesMagicAtFirstTwoOffsets) {
    const SerializedFrame bytes{serialize(make_frame())};

    EXPECT_EQ(bytes[0U], 0x54U);
    EXPECT_EQ(bytes[1U], 0x4CU);
}

TEST(SerializationTest, WritesVersionAtDefinedOffset) {
    const SerializedFrame bytes{serialize(make_frame())};

    EXPECT_EQ(bytes[version_offset], 0x01U);
}

TEST(SerializationTest, WritesMessageTypeAtDefinedOffset) {
    const SerializedFrame bytes{serialize(make_frame())};

    EXPECT_EQ(bytes[message_type_offset], 0x01U);
}

TEST(SerializationTest, WritesPayloadSizeInBigEndian) {
    TelemetryFrame frame{make_frame()};
    frame.header.payload_size = 0xFFFFU;
    const SerializedFrame bytes{serialize(frame)};

    EXPECT_EQ(bytes[payload_size_offset], 0x00U);
    EXPECT_EQ(bytes[payload_size_offset + 1U], 0x0CU);
}

TEST(SerializationTest, WritesSequenceInBigEndian) {
    const SerializedFrame bytes{serialize(make_frame())};

    EXPECT_EQ(bytes[sequence_offset], 0x12U);
    EXPECT_EQ(bytes[sequence_offset + 1U], 0x34U);
    EXPECT_EQ(bytes[sequence_offset + 2U], 0x56U);
    EXPECT_EQ(bytes[sequence_offset + 3U], 0x78U);
}

TEST(SerializationTest, WritesTimestampInBigEndian) {
    const SerializedFrame bytes{serialize(make_frame())};

    constexpr std::array<std::uint8_t, 8U> expected{
        0x01U, 0x02U, 0x03U, 0x04U, 0x05U, 0x06U, 0x07U, 0x08U};
    for (std::size_t index{0U}; index < expected.size(); ++index) {
        EXPECT_EQ(bytes[timestamp_offset + index], expected[index]);
    }
}

TEST(RoundTripTest, PreservesPositiveTemperature) {
    TelemetryFrame frame{make_frame()};
    frame.payload.temperature_millicelsius = 42'750;
    const ParseResult result{round_trip(frame)};

    ASSERT_TRUE(result.frame.has_value());
    EXPECT_EQ(result.frame->payload.temperature_millicelsius, 42'750);
}

TEST(RoundTripTest, PreservesNegativeTemperature) {
    TelemetryFrame frame{make_frame()};
    frame.payload.temperature_millicelsius = -40'125;
    const ParseResult result{round_trip(frame)};

    ASSERT_TRUE(result.frame.has_value());
    EXPECT_EQ(result.frame->payload.temperature_millicelsius, -40'125);
}

TEST(RoundTripTest, PreservesPressure) {
    TelemetryFrame frame{make_frame()};
    frame.payload.pressure_pascal = 250'001U;
    const ParseResult result{round_trip(frame)};

    ASSERT_TRUE(result.frame.has_value());
    EXPECT_EQ(result.frame->payload.pressure_pascal, 250'001U);
}

TEST(RoundTripTest, PreservesSupplyVoltage) {
    TelemetryFrame frame{make_frame()};
    frame.payload.supply_millivolts = 5'000U;
    const ParseResult result{round_trip(frame)};

    ASSERT_TRUE(result.frame.has_value());
    EXPECT_EQ(result.frame->payload.supply_millivolts, 5'000U);
}

TEST(RoundTripTest, PreservesStatusFlags) {
    TelemetryFrame frame{make_frame()};
    frame.payload.status_flags =
        static_cast<std::uint16_t>(StatusFlag::voltage_warning) |
        static_cast<std::uint16_t>(StatusFlag::communication_warning);
    const ParseResult result{round_trip(frame)};

    ASSERT_TRUE(result.frame.has_value());
    EXPECT_EQ(result.frame->payload.status_flags, frame.payload.status_flags);
}

TEST(RoundTripTest, PreservesCompleteFrame) {
    const TelemetryFrame frame{make_frame()};
    const ParseResult result{round_trip(frame)};

    ASSERT_TRUE(result.frame.has_value());
    EXPECT_EQ(*result.frame, frame);
    EXPECT_EQ(result.error, ParseError::none);
}

TEST(SerializationTest, IsDeterministic) {
    const TelemetryFrame frame{make_frame()};

    EXPECT_EQ(serialize(frame), serialize(frame));
}

TEST(DeserializationTest, RejectsEmptyFrame) {
    const ParseResult result{
        deserialize(std::span<const std::uint8_t>{})};

    EXPECT_EQ(result.error, ParseError::invalid_frame_size);
    EXPECT_FALSE(result.frame.has_value());
}

TEST(DeserializationTest, RejectsTruncatedFrame) {
    const SerializedFrame bytes{serialize(make_frame())};
    const ParseResult result{
        deserialize(std::span<const std::uint8_t>{bytes}.first(frame_size - 1U))};

    EXPECT_EQ(result.error, ParseError::invalid_frame_size);
}

TEST(DeserializationTest, RejectsOversizedFrame) {
    std::array<std::uint8_t, frame_size + 1U> bytes{};

    const ParseResult result{deserialize(bytes)};

    EXPECT_EQ(result.error, ParseError::invalid_frame_size);
}

TEST(DeserializationTest, RejectsInvalidMagic) {
    SerializedFrame bytes{serialize(make_frame())};
    bytes[0U] ^= 0x01U;

    const ParseResult result{deserialize(bytes)};

    EXPECT_EQ(result.error, ParseError::invalid_magic);
}

TEST(DeserializationTest, RejectsUnsupportedVersion) {
    SerializedFrame bytes{serialize(make_frame())};
    bytes[version_offset] = 0x02U;

    const ParseResult result{deserialize(bytes)};

    EXPECT_EQ(result.error, ParseError::unsupported_version);
}

TEST(DeserializationTest, RejectsUnsupportedMessageType) {
    SerializedFrame bytes{serialize(make_frame())};
    bytes[message_type_offset] = 0x02U;

    const ParseResult result{deserialize(bytes)};

    EXPECT_EQ(result.error, ParseError::unsupported_message_type);
}

TEST(DeserializationTest, RejectsInvalidPayloadSize) {
    SerializedFrame bytes{serialize(make_frame())};
    bytes[payload_size_offset + 1U] = 0x0BU;

    const ParseResult result{deserialize(bytes)};

    EXPECT_EQ(result.error, ParseError::invalid_payload_size);
}

TEST(DeserializationTest, DetectsCorruptedHeaderByte) {
    SerializedFrame bytes{serialize(make_frame())};
    bytes[sequence_offset] ^= 0x01U;

    const ParseResult result{deserialize(bytes)};

    EXPECT_EQ(result.error, ParseError::checksum_mismatch);
}

TEST(DeserializationTest, DetectsCorruptedPayloadByte) {
    SerializedFrame bytes{serialize(make_frame())};
    bytes[pressure_offset] ^= 0x01U;

    const ParseResult result{deserialize(bytes)};

    EXPECT_EQ(result.error, ParseError::checksum_mismatch);
}

TEST(DeserializationTest, DetectsCorruptedCrc) {
    SerializedFrame bytes{serialize(make_frame())};
    bytes[crc_offset + crc_size - 1U] ^= 0x01U;

    const ParseResult result{deserialize(bytes)};

    EXPECT_EQ(result.error, ParseError::checksum_mismatch);
}

TEST(RoundTripTest, PreservesMinimumValues) {
    TelemetryFrame frame{make_frame()};
    frame.header.sequence = std::numeric_limits<std::uint32_t>::min();
    frame.header.timestamp_microseconds =
        std::numeric_limits<std::uint64_t>::min();
    frame.payload.temperature_millicelsius =
        std::numeric_limits<std::int32_t>::min();
    frame.payload.pressure_pascal = std::numeric_limits<std::uint32_t>::min();
    frame.payload.supply_millivolts =
        std::numeric_limits<std::uint16_t>::min();
    frame.payload.status_flags = std::numeric_limits<std::uint16_t>::min();
    const ParseResult result{round_trip(frame)};

    ASSERT_TRUE(result.frame.has_value());
    EXPECT_EQ(*result.frame, frame);
}

TEST(RoundTripTest, PreservesMaximumValues) {
    TelemetryFrame frame{make_frame()};
    frame.header.sequence = std::numeric_limits<std::uint32_t>::max();
    frame.header.timestamp_microseconds =
        std::numeric_limits<std::uint64_t>::max();
    frame.payload.temperature_millicelsius =
        std::numeric_limits<std::int32_t>::max();
    frame.payload.pressure_pascal = std::numeric_limits<std::uint32_t>::max();
    frame.payload.supply_millivolts =
        std::numeric_limits<std::uint16_t>::max();
    frame.payload.status_flags = std::numeric_limits<std::uint16_t>::max();
    const ParseResult result{round_trip(frame)};

    ASSERT_TRUE(result.frame.has_value());
    EXPECT_EQ(*result.frame, frame);
}

TEST(ParseResultTest, ReportsValueForSuccessfulParse) {
    const ParseResult result{round_trip(make_frame())};

    EXPECT_TRUE(result.has_value());
}

TEST(ParseResultTest, ReportsNoValueForParseError) {
    const ParseResult result{
        deserialize(std::span<const std::uint8_t>{})};

    EXPECT_FALSE(result.has_value());
}

}  // namespace
}  // namespace telemetry::protocol
