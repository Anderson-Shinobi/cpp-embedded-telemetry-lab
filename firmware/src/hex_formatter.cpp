#include "telemetry_firmware/hex_formatter.hpp"

#include <cstddef>
#include <cstdint>

namespace telemetry::firmware {
namespace {

constexpr char hexadecimal_digits[]{"0123456789ABCDEF"};

}  // namespace

HexFormatter::Buffer HexFormatter::format(
    const telemetry::protocol::SerializedFrame& frame) noexcept {
    Buffer output{};

    for (std::size_t index{0U}; index < frame.size(); ++index) {
        const std::uint8_t byte{frame[index]};
        output[index * 2U] = hexadecimal_digits[byte >> 4U];
        output[(index * 2U) + 1U] = hexadecimal_digits[byte & 0x0FU];
    }

    output[hexadecimal_frame_length] = '\0';
    return output;
}

}  // namespace telemetry::firmware
