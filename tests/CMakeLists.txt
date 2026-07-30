include(FetchContent)

set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.17.0
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(googletest)

add_executable(
    protocol_tests
    protocol/protocol_tests.cpp
)

target_compile_features(protocol_tests PRIVATE cxx_std_20)
target_link_libraries(
    protocol_tests
    PRIVATE
        telemetry::protocol
        GTest::gtest_main
)

telemetry_enable_strict_warnings(protocol_tests)

include(GoogleTest)
gtest_discover_tests(
    protocol_tests
    DISCOVERY_MODE PRE_TEST
)

function(add_telemetry_host_test target_name source_file)
    add_executable(${target_name} ${source_file})
    target_compile_features(${target_name} PRIVATE cxx_std_20)
    target_link_libraries(
        ${target_name}
        PRIVATE
            telemetry::host
            GTest::gtest_main
    )
    telemetry_enable_strict_warnings(${target_name})
    gtest_discover_tests(
        ${target_name}
        DISCOVERY_MODE PRE_TEST
    )
endfunction()

add_telemetry_host_test(
    frame_ingestor_tests
    host/frame_ingestor_tests.cpp
)
add_telemetry_host_test(
    telemetry_metrics_tests
    host/telemetry_metrics_tests.cpp
)
add_telemetry_host_test(
    telemetry_pipeline_tests
    host/telemetry_pipeline_tests.cpp
)
add_telemetry_host_test(
    telemetry_report_tests
    host/telemetry_report_tests.cpp
)

add_test(
    NAME telemetry_host_demo_smoke
    COMMAND telemetry_host_demo
)
set_tests_properties(
    telemetry_host_demo_smoke
    PROPERTIES
        PASS_REGULAR_EXPRESSION "Telemetry Host Pipeline Report"
        TIMEOUT 10
)
