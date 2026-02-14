cmake_minimum_required(VERSION 3.15)

project(arena_hnswlib LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include directories
include_directories(include)

# Add library (header-only)

add_library(arena-hnswlib INTERFACE)
target_include_directories(arena-hnswlib INTERFACE include)

# Enable AVX for all targets

# Enable SSE2 and AVX for all targets (fixes __m128i/__m128d errors)

# Enable SSE2, AVX, and force SSE math for all targets (fixes __m128i/__m128d errors)
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-msse2 -mavx -O3 -g)
endif()

# Add tests

enable_testing()
add_subdirectory(tests/unit)

# Add benchmarks

add_executable(benchmark_math_utils benchmarks/benchmark_math_utils.cpp)
target_link_libraries(benchmark_math_utils arena-hnswlib benchmark)


add_executable(benchmark_index benchmarks/benchmark_index.cpp)
target_link_libraries(benchmark_index arena-hnswlib benchmark)

# Include FetchContent module
include(FetchContent)

# Fetch GoogleTest
FetchContent_Declare(
    googletest
    #URL https://github.com/google/googletest/archive/refs/tags/v1.16.0.zip
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.16.0
)
FetchContent_MakeAvailable(googletest)

# Fetch Google Benchmark
FetchContent_Declare(
    googlebenchmark
    #URL https://github.com/google/benchmark/archive/refs/tags/v1.9.0.zip
    GIT_REPOSITORY https://github.com/google/benchmark.git
    GIT_TAG v1.9.0
)
FetchContent_MakeAvailable(googlebenchmark)

# Link GoogleTest to test target
target_link_libraries(test_math_utils arena-hnswlib gtest gtest_main)

# Link Google Benchmark to benchmark target
target_link_libraries(benchmark_math_utils arena-hnswlib benchmark)