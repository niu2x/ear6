add_executable(ear6-desktop main.cpp)

target_link_libraries(ear6-desktop PRIVATE ear6 Threads::Threads)

cxx_project_preset(ear6-desktop)
warning_as_error_enable(ear6-desktop)

set_target_properties(ear6-desktop PROPERTIES INSTALL_RPATH "$ORIGIN/../lib")

install(TARGETS ear6-desktop RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
