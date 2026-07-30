#pragma once

#include <cstdint>
#include <span>

namespace telemetry::protocol {

[[nodiscard]] std::uint32_t calculate_crc32(
    std::span<const std::uint8_t> bytes) noexcept;

}  // namespace telemetry::protocol
