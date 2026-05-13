find_package(Boost REQUIRED COMPONENTS program_options)

add_executable(ear6-cli
    main.cpp
)

target_link_libraries(ear6-cli PRIVATE
    ear6
    Boost::program_options
)

cxx_project_preset(ear6-cli)
warning_as_error_enable(ear6-cli)

set_target_properties(ear6-cli PROPERTIES INSTALL_RPATH "$ORIGIN/../lib")

install(TARGETS ear6-cli RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
