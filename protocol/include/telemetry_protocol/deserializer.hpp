#pragma once

#include "telemetry_protocol/protocol_types.hpp"

#include <cstdint>
#include <optional>
#include <span>

namespace telemetry::protocol {

enum class ParseError {
    none,
    invalid_frame_size,
    invalid_magic,
    unsupported_version,
    unsupported_message_type,
    invalid_payload_size,
    checksum_mismatch,
};

struct ParseResult {
    std::optional<TelemetryFrame> frame{};
    ParseError error{ParseError::none};

    [[nodiscard]] bool has_value() const noexcept;
};

[[nodiscard]] ParseResult deserialize(
    std::span<const std::uint8_t> bytes) noexcept;

}  // namespace telemetry::protocol
