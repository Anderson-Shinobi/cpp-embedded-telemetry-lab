find_package(Threads REQUIRED)
find_package(cpp_safe_concurrent_buffer 0.3 REQUIRED)

add_library(
    telemetry_host_core
    src/frame_ingestor.cpp
    src/simulated_frame_source.cpp
    src/telemetry_metrics.cpp
    src/telemetry_processor.cpp
    src/telemetry_report.cpp
)

add_library(telemetry::host ALIAS telemetry_host_core)

target_compile_features(telemetry_host_core PUBLIC cxx_std_20)

target_include_directories(
    telemetry_host_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(
    telemetry_host_core
    PUBLIC
        telemetry::protocol
        cpp_safe_concurrent_buffer::concurrent_buffer
)

telemetry_enable_strict_warnings(telemetry_host_core)

add_executable(
    telemetry_host_demo
    src/main.cpp
)

target_compile_features(telemetry_host_demo PRIVATE cxx_std_20)
target_link_libraries(
    telemetry_host_demo
    PRIVATE
        telemetry::host
        Threads::Threads
)

telemetry_enable_strict_warnings(telemetry_host_demo)
