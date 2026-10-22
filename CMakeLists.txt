cmake_minimum_required(VERSION 3.20)

project(EventlyReflection VERSION 3.0.0 LANGUAGES CXX)

if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" CACHE STRING "MSVC runtime library")
endif()

set(EVENTLY_CXX_STANDARD "11" CACHE STRING "C++ standard: 11, 14, or 17")
set_property(CACHE EVENTLY_CXX_STANDARD PROPERTY STRINGS 11 14 17)

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

add_library(evently_reflection STATIC
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
evently_target_defaults(evently_reflection)

add_executable(Test
    main.cpp
)
target_link_libraries(Test PRIVATE evently_reflection)
evently_target_defaults(Test)

include(CTest)
if(BUILD_TESTING)
    find_package(Threads REQUIRED)
    add_executable(ReflectionTests
        tests/reflection_tests.cpp
    )
    target_link_libraries(ReflectionTests PRIVATE evently_reflection Threads::Threads)
    evently_target_defaults(ReflectionTests)
    add_test(NAME reflection_tests COMMAND ReflectionTests)
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
