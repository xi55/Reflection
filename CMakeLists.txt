cmake_minimum_required(VERSION 3.5)
project(Reflection)

# 设置C++11标准
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 添加编译选项
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")

# 添加可执行文件
add_executable(Test
    main.cpp
    Reflection.cpp
)

# 包含头文件目录
target_include_directories(Test PRIVATE .)