find_package(Qt6 COMPONENTS Widgets)

if(NOT Qt6_FOUND)
    message(STATUS "Qt6 not found - desktop app will not be built. Install Qt6 to enable it.")
    set(EAR6_BUILD_DESKTOP OFF CACHE BOOL "Build desktop app" FORCE)
    return()
endif()

set(CMAKE_AUTOMOC ON)

add_executable(ear6-desktop
    main.cpp
    main_window.h
    main_window.cpp
    emulator_widget.h
    emulator_widget.cpp
)

target_link_libraries(ear6-desktop PRIVATE
    ear6
    Qt6::Widgets
    Threads::Threads
)

cxx_project_preset(ear6-desktop)
warning_as_error_enable(ear6-desktop)

set_target_properties(ear6-desktop PROPERTIES INSTALL_RPATH "$ORIGIN/../lib")

install(TARGETS ear6-desktop RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
