#include "telemetry_firmware/deterministic_sensor_model.hpp"
#include "telemetry_firmware/firmware_metrics.hpp"
#include "telemetry_firmware/telemetry_producer.hpp"
#include "telemetry_firmware/telemetry_queue.hpp"
#include "telemetry_firmware/telemetry_transmitter.hpp"

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#if defined(CONFIG_BOARD_NATIVE_SIM)
#include <nsi_main.h>
#endif

#include <cstddef>
#include <cstdint>

namespace {

constexpr std::size_t producer_stack_size{2'048U};
constexpr std::size_t transmitter_stack_size{2'048U};
constexpr int producer_priority{5};
constexpr int transmitter_priority{4};

K_THREAD_STACK_DEFINE(producer_stack, producer_stack_size);
K_THREAD_STACK_DEFINE(transmitter_stack, transmitter_stack_size);
struct k_thread producer_thread_data;
struct k_thread transmitter_thread_data;
K_SEM_DEFINE(producer_finished, 0, 1);
K_SEM_DEFINE(transmitter_finished, 0, 1);

struct ProducerThreadContext {
    telemetry::firmware::TelemetryProducer* producer{};
    bool succeeded{};
};

struct TransmitterThreadContext {
    telemetry::firmware::TelemetryTransmitter* transmitter{};
    bool succeeded{};
};

void producer_thread_entry(void* first, void*, void*) {
    auto* const context{static_cast<ProducerThreadContext*>(first)};
    context->succeeded = context->producer->run();
    k_sem_give(&producer_finished);
}

void transmitter_thread_entry(void* first, void*, void*) {
    auto* const context{static_cast<TransmitterThreadContext*>(first)};
    context->succeeded = context->transmitter->run();
    k_sem_give(&transmitter_finished);
}

bool metrics_are_valid(
    const telemetry::firmware::FirmwareMetricsSnapshot& metrics) noexcept {
    return metrics.samples_generated ==
               telemetry::firmware::deterministic_sample_count &&
           metrics.frames_serialized ==
               telemetry::firmware::deterministic_sample_count &&
           metrics.frames_enqueued ==
               telemetry::firmware::deterministic_sample_count &&
           metrics.frames_transmitted ==
               telemetry::firmware::deterministic_sample_count &&
           metrics.queue_push_failures == 0U &&
           metrics.queue_pop_failures == 0U &&
           metrics.end_markers_sent == 1U &&
           metrics.end_markers_received == 1U;
}

}  // namespace

int main() {
    telemetry::firmware::DeterministicSensorModel sensor_model{};
    telemetry::firmware::TelemetryQueue queue{};
    telemetry::firmware::FirmwareMetrics metrics{};
    telemetry::firmware::TelemetryProducer producer{
        sensor_model, queue, metrics};
    telemetry::firmware::TelemetryTransmitter transmitter{queue, metrics};
    ProducerThreadContext producer_context{.producer = &producer};
    TransmitterThreadContext transmitter_context{.transmitter = &transmitter};

    k_tid_t transmitter_id{k_thread_create(
        &transmitter_thread_data,
        transmitter_stack,
        K_THREAD_STACK_SIZEOF(transmitter_stack),
        transmitter_thread_entry,
        &transmitter_context,
        nullptr,
        nullptr,
        transmitter_priority,
        0,
        K_NO_WAIT)};
    k_tid_t producer_id{k_thread_create(
        &producer_thread_data,
        producer_stack,
        K_THREAD_STACK_SIZEOF(producer_stack),
        producer_thread_entry,
        &producer_context,
        nullptr,
        nullptr,
        producer_priority,
        0,
        K_NO_WAIT)};

    (void)k_thread_name_set(transmitter_id, "telemetry_tx");
    (void)k_thread_name_set(producer_id, "telemetry_producer");

    (void)k_sem_take(&producer_finished, K_FOREVER);
    (void)k_sem_take(&transmitter_finished, K_FOREVER);

    const telemetry::firmware::FirmwareMetricsSnapshot final_metrics{
        metrics.snapshot()};
    const std::uint64_t queue_errors{
        final_metrics.queue_push_failures +
        final_metrics.queue_pop_failures};
    const bool succeeded{
        producer_context.succeeded && transmitter_context.succeeded &&
        metrics_are_valid(final_metrics) && queue.empty()};

    if (!succeeded) {
        printk(
            "TLFIRMWARE ERROR produced=%llu transmitted=%llu "
            "queue_errors=%llu\n",
            static_cast<unsigned long long>(final_metrics.frames_enqueued),
            static_cast<unsigned long long>(final_metrics.frames_transmitted),
            static_cast<unsigned long long>(queue_errors));
    } else {
        printk(
            "TLFIRMWARE SUMMARY produced=%llu transmitted=%llu "
            "queue_errors=%llu\n",
            static_cast<unsigned long long>(final_metrics.frames_enqueued),
            static_cast<unsigned long long>(final_metrics.frames_transmitted),
            static_cast<unsigned long long>(queue_errors));
        printk("TLFIRMWARE DONE\n");
    }

    const int exit_code{succeeded ? 0 : 1};
#if defined(CONFIG_BOARD_NATIVE_SIM)
    nsi_exit(exit_code);
#else
    return exit_code;
#endif
}
