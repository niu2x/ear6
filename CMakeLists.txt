cmake_minimum_required(VERSION 3.17.0)

project(Ear6 VERSION 0.1.0 LANGUAGES CXX C)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

include(cxx_project)
include(warning_as_error)

message(STATUS "Build for ${CMAKE_SYSTEM_NAME}")

# ==================== 构建选项 ====================

option(EAR6_BUILD_DESKTOP "Build desktop app (Windows/macOS/Linux, requires Qt6)" ON)
option(EAR6_BUILD_WEB "Build web app (requires Emscripten)" OFF)

# ==================== 依赖查找 ====================

find_package(Threads REQUIRED)

# ==================== 配置文件生成 ====================

configure_file(
    "${CMAKE_SOURCE_DIR}/include/ear6/version.h.in"
    "${CMAKE_BINARY_DIR}/ear6/version.h"
)

set(EAR6_NES_DB_TXT "${CMAKE_SOURCE_DIR}/assets/nes/nes_db.txt")
set(EAR6_NES_DB_EMBEDDED_H "${CMAKE_BINARY_DIR}/generated/nes_db_embedded.h")

add_custom_command(
    OUTPUT "${EAR6_NES_DB_EMBEDDED_H}"
    COMMAND ${CMAKE_COMMAND}
        -DINPUT_FILE=${EAR6_NES_DB_TXT}
        -DOUTPUT_FILE=${EAR6_NES_DB_EMBEDDED_H}
        -P "${CMAKE_SOURCE_DIR}/cmake/embed_nes_db.cmake"
    DEPENDS "${EAR6_NES_DB_TXT}" "${CMAKE_SOURCE_DIR}/cmake/embed_nes_db.cmake"
    VERBATIM
)

add_custom_target(ear6_generate_nes_db_header DEPENDS "${EAR6_NES_DB_EMBEDDED_H}")

# ==================== 库构建 ====================

if(CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
    set(EAR6_LIBRARY_TYPE STATIC)
else()
    set(EAR6_LIBRARY_TYPE SHARED)
endif()

add_library(ear6 ${EAR6_LIBRARY_TYPE}
    src/ear6.cpp
    src/system_test.cpp
    src/blip_buf.c
    src/nes/ines_loader.cpp
    src/nes/mapper_factory.cpp
    src/nes/mapper_000.cpp
    src/nes/mapper_001.cpp
    src/nes/mapper_002.cpp
    src/nes/mapper_003.cpp
    src/nes/mapper_004.cpp
    src/nes/mapper_005.cpp
    src/nes/mapper_006.cpp
    src/nes/mapper_007.cpp
    src/nes/base_mapper.cpp
    src/nes/nes_memory_manager.cpp
    src/nes/nes_cpu.cpp
    src/nes/nes_ppu.cpp
    src/nes/nes_apu.cpp
    src/nes/nes_sound_mixer.cpp
    src/nes/square_channel.cpp
    src/nes/triangle_channel.cpp
    src/nes/noise_channel.cpp
    src/nes/delta_modulation_channel.cpp
    src/nes/apu_length_counter.cpp
    src/nes/apu_frame_counter.cpp
    src/nes/nes_control_manager.cpp
    src/nes/vs_control_manager.cpp
    src/nes/nes_console.cpp
    src/nes/nes_system.cpp
    "${EAR6_NES_DB_EMBEDDED_H}"
)

add_dependencies(ear6 ear6_generate_nes_db_header)

target_include_directories(ear6 PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}>
    $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/generated>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(ear6 PRIVATE Threads::Threads)

cxx_project_preset(ear6)
warning_as_error_enable(ear6)

# ==================== 导出头文件生成 ====================

include(GenerateExportHeader)
generate_export_header(ear6 EXPORT_MACRO_NAME EAR6_API EXPORT_FILE_NAME ear6/export.h)

# ==================== 应用构建 ====================

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
    add_subdirectory(app/cli)
endif()

if(EAR6_BUILD_DESKTOP)
    add_subdirectory(app/desktop)
endif()

# ==================== 测试构建 ====================

if(EAR6_BUILD_TESTS)
    enable_testing()
    find_package(GTest REQUIRED)
    find_package(OpenSSL REQUIRED)
    add_executable(ear6-test tests/test_ear6.cpp)
    target_compile_definitions(ear6-test PRIVATE EAR6_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    target_include_directories(ear6-test PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_BINARY_DIR})
    target_link_libraries(ear6-test PRIVATE ear6 GTest::GTest GTest::Main OpenSSL::Crypto)
    add_test(NAME ear6-test COMMAND ear6-test)
endif()

if(EAR6_BUILD_WEB)
    if(NOT CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
        message(FATAL_ERROR "Web build requires Emscripten. Use emcmake cmake to configure.")
    endif()
    add_subdirectory(app/web)
endif()

# ==================== 安装配置 ====================

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

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
