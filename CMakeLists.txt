cmake_minimum_required(VERSION 3.20)

project(EventlyReflection VERSION 4.2.0 LANGUAGES CXX)

if(MSVC AND NOT BUILD_SHARED_LIBS)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" CACHE STRING "MSVC runtime library")
endif()

set(EVENTLY_CXX_STANDARD "11" CACHE STRING "C++ standard: 11, 14, or 17")
set_property(CACHE EVENTLY_CXX_STANDARD PROPERTY STRINGS 11 14 17)
option(EVENTLY_BUILD_EXAMPLES "Build example programs" ON)
option(EVENTLY_BUILD_BENCHMARKS "Build benchmarks" OFF)
option(EVENTLY_BUILD_DOCS "Build Doxygen documentation" OFF)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

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

add_library(evently_reflection
    Reflection.cpp
)
add_library(Evently::Reflection ALIAS evently_reflection)
set_target_properties(evently_reflection PROPERTIES EXPORT_NAME Reflection)
target_include_directories(evently_reflection PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_compile_features(evently_reflection PUBLIC cxx_std_11)
if(MSVC)
    target_compile_options(evently_reflection INTERFACE /utf-8)
endif()
if(BUILD_SHARED_LIBS)
    target_compile_definitions(evently_reflection PRIVATE EVENTLY_BUILDING_LIBRARY)
    target_compile_definitions(evently_reflection PUBLIC EVENTLY_SHARED)
endif()
evently_target_defaults(evently_reflection)

add_executable(Test
    main.cpp
)
target_link_libraries(Test PRIVATE evently_reflection)
evently_target_defaults(Test)

if(EVENTLY_BUILD_EXAMPLES)
    add_executable(JsonSerializationExample
        examples/json_serialization.cpp
    )
    target_link_libraries(JsonSerializationExample PRIVATE evently_reflection)
    evently_target_defaults(JsonSerializationExample)
endif()

if(EVENTLY_BUILD_BENCHMARKS)
    add_executable(ReflectionBenchmark
        benchmarks/reflection_benchmark.cpp
    )
    target_link_libraries(ReflectionBenchmark PRIVATE evently_reflection)
    evently_target_defaults(ReflectionBenchmark)
endif()

include(CTest)
if(BUILD_TESTING)
    find_package(Threads REQUIRED)
    add_executable(ReflectionTests
        tests/reflection_tests.cpp
    )
    target_link_libraries(ReflectionTests PRIVATE evently_reflection Threads::Threads)
    evently_target_defaults(ReflectionTests)
    add_test(NAME reflection_core COMMAND ReflectionTests --exclude ThreadSafety)
    add_test(NAME reflection_thread_safety COMMAND ReflectionTests --test ThreadSafety)
    if(EVENTLY_BUILD_EXAMPLES)
        add_test(NAME json_serialization_example COMMAND JsonSerializationExample)
    endif()
endif()

if(EVENTLY_BUILD_DOCS)
    find_package(Doxygen REQUIRED)
    configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/docs/Doxyfile.in
        ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
        @ONLY
    )
    add_custom_target(docs
        COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Generating Evently API documentation"
    )
endif()

install(TARGETS evently_reflection
    EXPORT EventlyTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(FILES
    Any.h
    EventlyExport.h
    IndexSequence.h
    ObjectHandle.h
    ReflectionError.h
    Reflection.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/Evently
)

install(EXPORT EventlyTargets
    FILE EventlyTargets.cmake
    NAMESPACE Evently::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Evently
)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/EventlyConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/EventlyConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Evently
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/EventlyConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/EventlyConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/EventlyConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Evently
)
