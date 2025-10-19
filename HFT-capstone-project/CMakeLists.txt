cmake_minimum_required(VERSION 3.14)
project(HFT_OrderBook VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler flags for optimization and warnings
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Optimization flags
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -DNDEBUG")
set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g -Wall -Wextra -Wpedantic")

# Add compiler warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
        -Wsign-conversion
        -Wno-unused-parameter
    )
endif()

# Source files
set(ORDER_BOOK_SOURCES
    order_book.cpp
)

set(ORDER_BOOK_HEADERS
    order_book.h
)

# Create library
add_library(order_book_lib STATIC ${ORDER_BOOK_SOURCES} ${ORDER_BOOK_HEADERS})
target_include_directories(order_book_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Main executable
add_executable(order_book_demo main.cpp)
target_link_libraries(order_book_demo PRIVATE order_book_lib)

# Enable optimization for release builds
if(CMAKE_BUILD_TYPE MATCHES Release)
    message(STATUS "Building in Release mode with optimizations")
    target_compile_definitions(order_book_demo PRIVATE NDEBUG)
endif()

# Print build information
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "C++ standard: C++${CMAKE_CXX_STANDARD}")

