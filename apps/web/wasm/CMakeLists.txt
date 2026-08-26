add_executable(ear6-wasm wasm_exports.cpp)

target_link_libraries(ear6-wasm PRIVATE ear6_a)

cxx_project_preset(ear6-wasm)
warning_as_error_enable(ear6-wasm)

set_target_properties(ear6-wasm PROPERTIES
    SUFFIX ".js"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/web-public/ear6"
)

string(CONCAT EAR6_LINK_FLAGS
    "-s MODULARIZE=1 "
    "-s EXPORT_NAME=createEar6 "
    "-s ALLOW_MEMORY_GROWTH=1 "
    "-s EXPORTED_RUNTIME_METHODS='[\"HEAPU8\",\"cwrap\"]' "
    "-s EXPORTED_FUNCTIONS='[\"_malloc\",\"_free\",\"_ear6_web_create\",\"_ear6_web_destroy\",\"_ear6_web_load_from_memory\",\"_ear6_web_save_state_to_memory\",\"_ear6_web_load_state_from_memory\",\"_ear6_web_get_state_identity\",\"_ear6_web_step\",\"_ear6_web_get_framebuffer\",\"_ear6_web_get_frame_width\",\"_ear6_web_get_frame_height\",\"_ear6_web_get_audiobuffer\",\"_ear6_web_get_audio_num_samples\",\"_ear6_web_consume_audio\",\"_ear6_web_nes_set_button_state\",\"_ear6_web_nes_clear_input\"]' "
    "-s EXCEPTION_CATCHING_ALLOWED=['*']"
)

set_target_properties(ear6-wasm PROPERTIES
    LINK_FLAGS "${EAR6_LINK_FLAGS}"
)

install(TARGETS ear6-wasm RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
