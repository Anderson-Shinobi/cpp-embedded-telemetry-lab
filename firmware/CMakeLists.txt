cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(cpp_embedded_telemetry_firmware LANGUAGES C CXX)

get_filename_component(PROJECT_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

target_sources(
    app
    PRIVATE
        src/deterministic_sensor_model.cpp
        src/firmware_metrics.cpp
        src/hex_formatter.cpp
        src/main.cpp
        src/telemetry_producer.cpp
        src/telemetry_queue.cpp
        src/telemetry_transmitter.cpp
        ${PROJECT_ROOT}/protocol/src/crc32.cpp
        ${PROJECT_ROOT}/protocol/src/deserializer.cpp
        ${PROJECT_ROOT}/protocol/src/serializer.cpp
)

target_include_directories(
    app
    PRIVATE
        include
        ${PROJECT_ROOT}/protocol/include
)

target_compile_features(app PRIVATE cxx_std_20)

target_compile_options(
    app
    PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:
            -Wall
            -Wextra
            -Wformat=2
            -Werror
            -ffile-prefix-map=${PROJECT_ROOT}=PROJECT_ROOT
        >
)

set(
    STRICT_CXX_SOURCES
    src/deterministic_sensor_model.cpp
    src/firmware_metrics.cpp
    src/hex_formatter.cpp
    src/telemetry_producer.cpp
    ${PROJECT_ROOT}/protocol/src/crc32.cpp
    ${PROJECT_ROOT}/protocol/src/deserializer.cpp
    ${PROJECT_ROOT}/protocol/src/serializer.cpp
)

set_source_files_properties(
    ${STRICT_CXX_SOURCES}
    PROPERTIES
        COMPILE_OPTIONS
            "-Wpedantic;-Wconversion;-Wsign-conversion;-Wshadow;-Wundef"
)
