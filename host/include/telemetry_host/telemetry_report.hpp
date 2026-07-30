#pragma once

#include "telemetry_host/telemetry_metrics.hpp"

#include <string>

namespace telemetry::host {

[[nodiscard]] std::string make_text_report(
    const TelemetryMetricsSnapshot& snapshot);

}  // namespace telemetry::host
