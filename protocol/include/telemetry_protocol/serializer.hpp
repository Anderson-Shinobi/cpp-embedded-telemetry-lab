#pragma once

#include "telemetry_protocol/protocol_types.hpp"

#include <array>
#include <cstdint>

namespace telemetry::protocol {

using SerializedFrame = std::array<std::uint8_t, frame_size>;

[[nodiscard]] SerializedFrame serialize(const TelemetryFrame& frame) noexcept;

}  // namespace telemetry::protocol
