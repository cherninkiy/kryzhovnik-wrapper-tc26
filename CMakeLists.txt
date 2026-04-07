
# Minimum CMake version and project definition
cmake_minimum_required(VERSION 3.10)
project(kryzhovnik C)

# Set C standard
set(CMAKE_C_STANDARD 99)

set(KRYZHOVNIK_PARAMSET "medium" CACHE STRING "Select a kryzhovnik parameter set")
set_property(CACHE KRYZHOVNIK_PARAMSET PROPERTY STRINGS "small" "medium" "large" "debug")
set(KRYZHOVNIK_BENCH_ITERS "200" CACHE STRING "Iterations for dedicated benchmark_tool target")
set(KRYZHOVNIK_BENCH_MLEN "32" CACHE STRING "Message length for dedicated benchmark_tool target")
option(KRYZHOVNIK_BUILD_BENCHMARK_TOOL "Build dedicated benchmark tool target" ON)
if(NOT KRYZHOVNIK_PARAMSET STREQUAL "small" AND
   NOT KRYZHOVNIK_PARAMSET STREQUAL "medium" AND
   NOT KRYZHOVNIK_PARAMSET STREQUAL "large" AND
   NOT KRYZHOVNIK_PARAMSET STREQUAL "debug")
    message(FATAL_ERROR "Unsupported KRYZHOVNIK_PARAMSET='${KRYZHOVNIK_PARAMSET}'. Use: small, medium, large, debug")
endif()
message(STATUS "Using Kryzhovnik parameter set ${KRYZHOVNIK_PARAMSET}")

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/current-params.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/current-params.h
)

# Source files for the main kryzhovnik static library
set(KRYZHOVNIK_SOURCES
    cpucycles.c
    fips202.c
    poly_mul.c
    randombytes.c
    rng.c
    sign.c
    speed.c
    kryzhovnik_wrapper.c
)

# Main kryzhovnik static library
add_library(kryzhovnik STATIC ${KRYZHOVNIK_SOURCES})
target_include_directories(kryzhovnik PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
)

# Wrapper static library (depends on kryzhovnik)
add_library(kryzhovnik_wrapper STATIC kryzhovnik_wrapper.c)
target_include_directories(kryzhovnik_wrapper PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
)
target_link_libraries(kryzhovnik_wrapper PRIVATE kryzhovnik)

# Test executable for the wrapper API
add_executable(test_wrapper test_wrapper.c)
target_link_libraries(test_wrapper PRIVATE kryzhovnik)
target_include_directories(test_wrapper PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
)

# Test executable for the wrapper (full test modes)
add_executable(test_pqs test_pqs.c)
target_link_libraries(test_pqs PRIVATE kryzhovnik)
target_include_directories(test_pqs PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
)

# List of all C source files (for other test targets)
set(SOURCES
    cpucycles.c
    fips202.c
    poly_mul.c
    pqs_test.c
    randombytes.c
    rng.c
    sign.c
    speed.c
)

# Header files (for IDEs and clarity)
set(HEADERS
    config.h
    cpucycles.h
    fips202.h
    params.h
    poly_mul.h
    randombytes.h
    rng.h
    sign.h
    speed.h
)

# Find OpenSSL for cryptographic routines
find_package(OpenSSL REQUIRED)

# Reference implementation test target (uses all sources)
add_executable(pqs_test pqs_test.c ${SOURCES})
target_link_libraries(pqs_test PRIVATE OpenSSL::Crypto)
target_include_directories(pqs_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
)

# Performance test target
add_executable(speed speed.c ${SOURCES})
target_link_libraries(speed PRIVATE OpenSSL::Crypto)
target_include_directories(speed PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
)

if(KRYZHOVNIK_BUILD_BENCHMARK_TOOL)
    add_executable(benchmark_tool benchmark_tools/benchmark_tool.c)
    target_link_libraries(benchmark_tool PRIVATE kryzhovnik)
    target_include_directories(benchmark_tool PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_BINARY_DIR}
    )
    target_compile_definitions(benchmark_tool PRIVATE
        BENCH_ITERS=${KRYZHOVNIK_BENCH_ITERS}
        BENCH_MLEN=${KRYZHOVNIK_BENCH_MLEN}
    )
endif()
