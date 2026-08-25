set(EAR6_NES_DB_TXT "${CMAKE_SOURCE_DIR}/data/nes/nes-db.txt")
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

set(EAR6_LIBRARY_SOURCES
    core/ear6.cpp
    core/state/state_container.upb.c
    core/state/state_container.upb_minitable.c
    systems/test/test_system.cpp
    systems/nes/ines_loader.cpp
    systems/nes/nes_memory_manager.cpp
    systems/nes/nes_cpu.cpp
    systems/nes/nes_ppu.cpp
    systems/nes/audio/nes_apu.cpp
    systems/nes/audio/nes_sound_mixer.cpp
    systems/nes/audio/square_channel.cpp
    systems/nes/audio/triangle_channel.cpp
    systems/nes/audio/noise_channel.cpp
    systems/nes/audio/delta_modulation_channel.cpp
    systems/nes/audio/apu_length_counter.cpp
    systems/nes/audio/apu_frame_counter.cpp
    systems/nes/nes_control_manager.cpp
    systems/nes/vs_control_manager.cpp
    systems/nes/nes_console.cpp
    systems/nes/nes_system.cpp
    systems/nes/nes_state.cpp
    "${CMAKE_SOURCE_DIR}/third_party/blip_buf/blip_buf.c"
    "${EAR6_NES_DB_EMBEDDED_H}"
)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
    add_library(ear6 SHARED ${EAR6_LIBRARY_SOURCES})
endif()
add_subdirectory(systems/nes/mappers)

if(NOT MSVC)
    set_source_files_properties(
        core/state/state_container.upb.c
        core/state/state_container.upb_minitable.c
        PROPERTIES COMPILE_OPTIONS "-Wno-unused-parameter"
    )
endif()

function(ear6_configure_library target)
    target_sources(${target} PRIVATE $<TARGET_OBJECTS:ear6_upb>)
    add_dependencies(${target} ear6_generate_nes_db_header)

    target_include_directories(${target} PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}>
        $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/generated>
        $<INSTALL_INTERFACE:include>
    )

    target_include_directories(${target} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/core
        ${CMAKE_CURRENT_SOURCE_DIR}/core/state
        ${CMAKE_CURRENT_SOURCE_DIR}/systems/nes
        ${CMAKE_CURRENT_SOURCE_DIR}/systems/nes/audio
        ${CMAKE_SOURCE_DIR}/third_party/blip_buf
    )
    target_include_directories(${target} SYSTEM PRIVATE
        ${CMAKE_SOURCE_DIR}/third_party/protobuf-36.0
        ${CMAKE_SOURCE_DIR}/third_party/protobuf-36.0/third_party/utf8_range
    )

    target_link_libraries(${target} PRIVATE Threads::Threads)

    if(EAR6_ENABLE_CPU_SEQ_TRACE)
        target_compile_definitions(${target} PRIVATE EAR6_ENABLE_CPU_SEQ_TRACE)
    endif()

    cxx_project_preset(${target})
    warning_as_error_enable(${target})
endfunction()

if(TARGET ear6)
    ear6_configure_library(ear6)
endif()

add_library(ear6_a STATIC ${EAR6_LIBRARY_SOURCES})
ear6_add_nes_mapper_sources(ear6_a)
ear6_configure_library(ear6_a)
target_compile_definitions(ear6_a PUBLIC EAR6_STATIC_DEFINE)
set_target_properties(ear6_a PROPERTIES OUTPUT_NAME ear6)

include(GenerateExportHeader)
if(TARGET ear6)
    set(EAR6_EXPORT_TARGET ear6)
else()
    set(EAR6_EXPORT_TARGET ear6_a)
endif()
generate_export_header(
    ${EAR6_EXPORT_TARGET}
    BASE_NAME ear6
    EXPORT_MACRO_NAME EAR6_API
    STATIC_DEFINE EAR6_STATIC_DEFINE
    EXPORT_FILE_NAME "${CMAKE_BINARY_DIR}/ear6/export.h"
)
