add_library(
    telemetry_protocol
    src/crc32.cpp
    src/deserializer.cpp
    src/serializer.cpp
)

add_library(telemetry::protocol ALIAS telemetry_protocol)

target_compile_features(telemetry_protocol PUBLIC cxx_std_20)

target_include_directories(
    telemetry_protocol
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

telemetry_enable_strict_warnings(telemetry_protocol)
