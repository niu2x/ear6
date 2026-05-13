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
    "-s EXPORTED_RUNTIME_METHODS='[\"HEAPU8\"]' "
    "-s EXPORTED_FUNCTIONS='[\"_malloc\",\"_free\"]' "
    "-s EXCEPTION_CATCHING_ALLOWED=['*']"
)

set_target_properties(ear6-web PROPERTIES
    LINK_FLAGS "${EAR6_LINK_FLAGS}"
)

install(TARGETS ear6-web RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
