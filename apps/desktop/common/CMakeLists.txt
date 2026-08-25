add_library(ear6-desktop-common STATIC
    audio_ring_buffer.cpp
    desktop_session.cpp
    save_store.cpp
)

target_include_directories(ear6-desktop-common PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_BINARY_DIR}
)

cxx_project_preset(ear6-desktop-common)
warning_as_error_enable(ear6-desktop-common)
