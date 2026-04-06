
cmake_minimum_required(VERSION 3.10)
project(kryzhovnik C CXX)


# Собираем все исходники как C++
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 99)


# Source files
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

# Компилируем все .c файлы как C++
set_source_files_properties(
    cpucycles.c
    fips202.c
    getCPUTime.c
    poly_mul.c
    pqs_test.c
    randombytes.c
    rng.c
    sign.c
    speed.c
    PROPERTIES LANGUAGE CXX
)

# Header files (for IDEs, not strictly needed by CMake)
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


# Собираем pqs_test и speed как C++-проекты, даже если исходники .c

find_package(OpenSSL REQUIRED)

add_executable(pqs_test pqs_test.c ${SOURCES})
set_target_properties(pqs_test PROPERTIES LINKER_LANGUAGE CXX)
target_link_libraries(pqs_test PRIVATE OpenSSL::Crypto)

add_executable(speed speed.c ${SOURCES})
set_target_properties(speed PROPERTIES LINKER_LANGUAGE CXX)
target_link_libraries(speed PRIVATE OpenSSL::Crypto)

target_include_directories(pqs_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(speed PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
