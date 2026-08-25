cmake_minimum_required(VERSION 3.17.0)

project(Ear6 VERSION 0.1.0 LANGUAGES CXX C)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

include(cxx_project)
include(warning_as_error)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

message(STATUS "Build for ${CMAKE_SYSTEM_NAME}")

option(EAR6_BUILD_DESKTOP "Build desktop app (Windows/macOS/Linux, requires Qt6)" ON)
option(EAR6_BUILD_WASM "Build WebAssembly bindings (requires Emscripten)" OFF)
option(EAR6_BUILD_TESTS "Build the Ear6 test suite" OFF)
option(EAR6_ENABLE_CPU_SEQ_TRACE "Enable per-instruction CPU trace (compile gate for EAR6_TRACE_CPU_SEQ env)" ON)

find_package(Threads REQUIRED)

add_subdirectory(third_party/protobuf-36.0)

configure_file(
    "${CMAKE_SOURCE_DIR}/include/ear6/version.h.in"
    "${CMAKE_BINARY_DIR}/ear6/version.h"
)

add_subdirectory(src)
add_subdirectory(apps)

if(EAR6_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

install(TARGETS ear6
    EXPORT Ear6Targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(EXPORT Ear6Targets
    FILE Ear6Targets.cmake
    NAMESPACE Ear6::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Ear6
)

configure_package_config_file(
    "${CMAKE_SOURCE_DIR}/cmake/Ear6Config.cmake.in"
    "${CMAKE_BINARY_DIR}/Ear6Config.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Ear6
)

write_basic_package_version_file(
    "${CMAKE_BINARY_DIR}/Ear6ConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    "${CMAKE_BINARY_DIR}/Ear6Config.cmake"
    "${CMAKE_BINARY_DIR}/Ear6ConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Ear6
)

install(FILES
    "${CMAKE_SOURCE_DIR}/include/ear6/ear6.h"
    "${CMAKE_SOURCE_DIR}/include/ear6/nes.h"
    "${CMAKE_SOURCE_DIR}/include/ear6/flash.h"
    "${CMAKE_BINARY_DIR}/ear6/version.h"
    "${CMAKE_BINARY_DIR}/ear6/export.h"
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/ear6
)
