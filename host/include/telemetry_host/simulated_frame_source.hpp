#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace telemetry::host {

using RawFrame = std::vector<std::uint8_t>;

inline constexpr std::size_t simulated_input_count{15U};
inline constexpr std::size_t simulated_valid_frame_count{8U};

class SimulatedFrameSource final {
public:
    [[nodiscard]] std::vector<RawFrame> generate() const;
};

}  // namespace telemetry::host
