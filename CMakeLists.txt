cmake_minimum_required(VERSION 3.0)
project(Mu)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++1y -stdlib=libc++")

set(SOURCE_FILES README.md)

include_directories(src)

# Performance benchmarking executables
add_executable(queue-perf  perf/mu/lf/queue.cpp)
add_executable(stack-perf  perf/mu/lf/stack.cpp)

# Test executables
add_executable(tst-heap tst/mu/adt/heap.cpp)
add_executable(tst-queue tst/mu/lf/queue.cpp)
