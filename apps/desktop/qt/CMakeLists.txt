find_package(Qt6 COMPONENTS Widgets)

if(NOT Qt6_FOUND)
    message(STATUS "Qt6 not found - desktop app will not be built. Install Qt6 to enable it.")
    return()
endif()

set(CMAKE_AUTOMOC ON)

add_executable(ear6-desktop-qt
    main.cpp
    main_window.h
    main_window.cpp
    emulator_widget.h
    emulator_widget.cpp
    save_store.h
    save_store.cpp
)

target_link_libraries(ear6-desktop-qt PRIVATE
    ear6
    Qt6::Widgets
    Threads::Threads
)

cxx_project_preset(ear6-desktop-qt)
warning_as_error_enable(ear6-desktop-qt)

set_target_properties(ear6-desktop-qt PROPERTIES
    OUTPUT_NAME ear6-desktop-qt
    INSTALL_RPATH "$ORIGIN/../lib"
)

install(TARGETS ear6-desktop-qt RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
