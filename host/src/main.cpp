#include "telemetry_host/frame_ingestor.hpp"
#include "telemetry_host/simulated_frame_source.hpp"
#include "telemetry_host/telemetry_metrics.hpp"
#include "telemetry_host/telemetry_processor.hpp"
#include "telemetry_host/telemetry_report.hpp"

#include <safe_concurrent_buffer.hpp>

#include <cstddef>
#include <exception>
#include <iostream>
#include <thread>

namespace {

constexpr std::size_t queue_capacity{3U};

}  // namespace

int main() {
    try {
        elite::concurrency::SafeConcurrentBuffer buffer{queue_capacity};
        telemetry::host::TelemetryMetrics metrics{};
        telemetry::host::FrameIngestor ingestor{buffer, metrics};
        telemetry::host::TelemetryProcessor processor{metrics};
        const telemetry::host::SimulatedFrameSource source{};

        telemetry::host::ConsumerResult consumer_result{};
        std::jthread consumer{[&buffer, &processor, &consumer_result]() {
            consumer_result =
                telemetry::host::consume_queued_frames(buffer, processor);
        }};

        bool ingestion_succeeded{true};
        try {
            for (const telemetry::host::RawFrame& raw_frame :
                 source.generate()) {
                const telemetry::host::IngestResult result{
                    ingestor.ingest(raw_frame)};
                if (result.status ==
                    telemetry::host::IngestStatus::queue_closed) {
                    ingestion_succeeded = false;
                    break;
                }
            }
        } catch (...) {
            buffer.close();
            consumer.join();
            throw;
        }

        buffer.close();
        consumer.join();

        const telemetry::host::TelemetryMetricsSnapshot snapshot{
            metrics.snapshot()};
        if (!ingestion_succeeded || !consumer_result.success ||
            consumer_result.frames_consumed != snapshot.frames_accepted) {
            std::cerr << "Telemetry host pipeline failed operationally\n";
            return 1;
        }

        std::cout << telemetry::host::make_text_report(snapshot);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Telemetry host pipeline failed: " << error.what() << '\n';
        return 1;
    } catch (...) {
        std::cerr << "Telemetry host pipeline failed unexpectedly\n";
        return 1;
    }
}
