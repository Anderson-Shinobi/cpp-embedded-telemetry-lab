cmake_minimum_required(VERSION 3.20)

project(
    embedded_telemetry_lab
    VERSION 0.1.0
    LANGUAGES CXX
)

option(BUILD_TESTING "Build tests" ON)

function(telemetry_enable_strict_warnings target_name)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(
            ${target_name}
            PRIVATE
                -Wall
                -Wextra
                -Wpedantic
                -Wconversion
                -Wsign-conversion
                -Wshadow
                -Wformat=2
                -Wundef
                -Werror
        )
    elseif(MSVC)
        target_compile_options(${target_name} PRIVATE /W4 /WX)
    endif()
endfunction()

include(CTest)

add_subdirectory(protocol)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
