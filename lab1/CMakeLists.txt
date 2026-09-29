cmake_minimum_required(VERSION 3.19)
project(lab1 VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(SRC_FILES
    src/measure.cpp
    src/metrics_collector.cpp
    src/zipf_random.cpp

    main.cpp
)

add_executable(lab1 ${SRC_FILES})
target_include_directories(lab1 PRIVATE include)
