cmake_minimum_required(VERSION 3.18)
project(LowLatencyBacktesting VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Optimization & System Flags
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(
        -O3
        -march=native
        -Wall
        -Wextra
        -Wpedantic
        -Werror
        -Wshadow
        -Wnon-virtual-dtor
        -funroll-loops
    )
endif()

# Fetch GoogleTest
include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
# For Windows MSVC runtime flexibility
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

include_directories(include)

# Executable
add_executable(low_latency_backtesting src/main.cpp)

# Add Benchmark Target
add_executable(bench_order_book benchmarks/bench_order_book.cpp)
target_include_directories(bench_order_book PRIVATE include)

# Apply release optimization flags for benchmarking (-O3 and native SIMD instructions)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(bench_order_book PRIVATE -O3 -march=native -ffast-math)
endif()

# Unit Test Target
enable_testing()
add_executable(unit_tests 
    tests/test_main.cpp
    tests/test_arena_pool.cpp
    tests/test_order_book.cpp
    tests/test_spsc_queue.cpp
    tests/test_shared_memory.cpp
)
target_link_libraries(unit_tests PRIVATE gtest_main)

include(GoogleTest)
gtest_discover_tests(unit_tests)