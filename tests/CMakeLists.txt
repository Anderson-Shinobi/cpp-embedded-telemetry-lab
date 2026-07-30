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
gtest_discover_tests(protocol_tests)
