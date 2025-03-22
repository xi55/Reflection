cmake_minimum_required(VERSION 3.10)
project(Test)

# 使用 C++17 标准
# todo: 兼容c++11, 结构体
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_definitions(-fpermissive)

include_directories(${CMAKE_SOURCE_DIR})

file(GLOB SOURCES ${CMAKE_SOURCE_DIR}/*.cpp)

add_executable(Test ${SOURCES})

target_link_libraries(Test pthread)