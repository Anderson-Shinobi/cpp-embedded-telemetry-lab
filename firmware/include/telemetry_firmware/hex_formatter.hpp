#pragma once

#include "telemetry_protocol/serializer.hpp"

#include <array>
#include <cstddef>

namespace telemetry::firmware {

inline constexpr std::size_t hexadecimal_frame_length{
    telemetry::protocol::frame_size * 2U};

class HexFormatter {
public:
    using Buffer = std::array<char, hexadecimal_frame_length + 1U>;

    [[nodiscard]] static Buffer format(
        const telemetry::protocol::SerializedFrame& frame) noexcept;
};

}  // namespace telemetry::firmware
