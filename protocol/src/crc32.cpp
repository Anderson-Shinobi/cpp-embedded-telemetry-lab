#include "telemetry_protocol/crc32.hpp"

#include <cstddef>

namespace telemetry::protocol {
namespace {

constexpr std::uint32_t polynomial{0xEDB88320U};
constexpr std::uint32_t initial_value{0xFFFFFFFFU};
constexpr std::uint32_t final_xor{0xFFFFFFFFU};
constexpr std::size_t bits_per_byte{8U};

}  // namespace

std::uint32_t calculate_crc32(
    const std::span<const std::uint8_t> bytes) noexcept {
    std::uint32_t crc{initial_value};

    for (const std::uint8_t byte : bytes) {
        crc ^= static_cast<std::uint32_t>(byte);
        for (std::size_t bit{0U}; bit < bits_per_byte; ++bit) {
            const std::uint32_t least_significant_bit{crc & 1U};
            const std::uint32_t mask{0U - least_significant_bit};
            crc = (crc >> 1U) ^ (polynomial & mask);
        }
    }

    return crc ^ final_xor;
}

}  // namespace telemetry::protocol
