cmake_minimum_required(VERSION 3.20)

project(EventlyReflection LANGUAGES CXX)

if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" CACHE STRING "MSVC runtime library")
endif()

set(EVENTLY_CXX_STANDARD "11" CACHE STRING "C++ standard: 11, 14, or 17")
set_property(CACHE EVENTLY_CXX_STANDARD PROPERTY STRINGS 11 14 17)

function(evently_target_defaults target)
    set_target_properties(${target} PROPERTIES
        CXX_STANDARD ${EVENTLY_CXX_STANDARD}
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
    )
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /utf-8 /Zc:__cplusplus)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

add_library(evently_reflection STATIC
    Reflection.cpp
)
target_include_directories(evently_reflection PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
evently_target_defaults(evently_reflection)

add_executable(Test
    main.cpp
)
target_link_libraries(Test PRIVATE evently_reflection)
evently_target_defaults(Test)

include(CTest)
if(BUILD_TESTING)
    add_executable(ReflectionTests
        tests/reflection_tests.cpp
    )
    target_link_libraries(ReflectionTests PRIVATE evently_reflection)
    evently_target_defaults(ReflectionTests)
    add_test(NAME reflection_tests COMMAND ReflectionTests)
endif()
