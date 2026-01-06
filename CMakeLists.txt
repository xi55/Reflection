cmake_minimum_required(VERSION 3.5)
## Prefer GCC/G++ when available.
# NOTE: CMake picks the compiler at configure time. Setting these variables here
# will only take effect if CMake hasn't already chosen a compiler (or if the
# cache is cleared). If your `build/` directory already has a CMake cache
# configured for MSVC, delete `build/CMakeCache.txt` (or remove `build/`) and
# re-run CMake to pick up GCC.
if(NOT DEFINED CMAKE_C_COMPILER)
    set(CMAKE_C_COMPILER gcc CACHE PATH "C compiler" FORCE)
endif()
if(NOT DEFINED CMAKE_CXX_COMPILER)
    set(CMAKE_CXX_COMPILER g++ CACHE PATH "C++ compiler" FORCE)
endif()

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