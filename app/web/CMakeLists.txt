add_executable(ear6-web main.cpp)

target_link_libraries(ear6-web PRIVATE ear6)

cxx_project_preset(ear6-web)
warning_as_error_enable(ear6-web)

set_target_properties(ear6-web PROPERTIES
    SUFFIX ".js"
)

string(CONCAT EAR6_LINK_FLAGS
    "-s MODULARIZE=1 "
    "-s EXPORT_NAME=createEar6 "
    "-s ALLOW_MEMORY_GROWTH=1 "
    "-s EXPORTED_RUNTIME_METHODS='[\"HEAPU8\",\"cwrap\"]' "
    "-s EXPORTED_FUNCTIONS='[\"_malloc\",\"_free\",\"_ear6_web_create\",\"_ear6_web_destroy\",\"_ear6_web_load_data\",\"_ear6_web_step\",\"_ear6_web_get_framebuffer\",\"_ear6_web_get_frame_width\",\"_ear6_web_get_frame_height\",\"_ear6_web_nes_set_button_state\",\"_ear6_web_nes_clear_input\"]' "
    "-s EXCEPTION_CATCHING_ALLOWED=['*']"
)

set_target_properties(ear6-web PROPERTIES
    LINK_FLAGS "${EAR6_LINK_FLAGS}"
)

install(TARGETS ear6-web RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
