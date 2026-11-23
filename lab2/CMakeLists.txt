cmake_minimum_required(VERSION 3.20)
project(lab2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Threads REQUIRED)

add_executable(lab2
    student-cpp/src/main.cpp
    student-cpp/src/scheduler.cpp
)
add_executable(stress student-cpp/extra/stress.cpp)
add_executable(ministress student-cpp/extra/ministress.cpp)

foreach(target lab2 stress ministress)
    target_link_libraries(${target} PRIVATE Threads::Threads)
endforeach()

option(ENABLE_TSAN "Build with ThreadSanitizer" OFF)
if(ENABLE_TSAN)
    target_compile_options(lab2 PRIVATE -fsanitize=thread -g -O1)
    target_link_options(lab2 PRIVATE -fsanitize=thread)
endif()
