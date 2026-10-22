cmake_minimum_required(VERSION 3.20)
project(EventlyConsumer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

find_package(Evently CONFIG REQUIRED)

add_executable(evently_consumer main.cpp)
target_link_libraries(evently_consumer PRIVATE Evently::Reflection)
