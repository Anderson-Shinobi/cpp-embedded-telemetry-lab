#pragma once

#include "telemetry_protocol/protocol_types.hpp"

#include <cstddef>
#include <cstdint>

namespace telemetry::firmware {

inline constexpr std::size_t deterministic_sample_count{8U};
inline constexpr std::uint32_t initial_sequence{1000U};
inline constexpr std::uint64_t initial_timestamp_microseconds{1'000'000U};
inline constexpr std::uint64_t timestamp_increment_microseconds{250'000U};

class DeterministicSensorModel {
public:
    [[nodiscard]] bool has_next() const noexcept;
    [[nodiscard]] telemetry::protocol::TelemetryFrame next() noexcept;
    [[nodiscard]] std::size_t remaining() const noexcept;

private:
    std::size_t next_index_{0U};
};

}  // namespace telemetry::firmware
