add_subdirectory(common)

if(APPLE)
    add_subdirectory(macos)
endif()

if(EAR6_BUILD_QT_DESKTOP)
    add_subdirectory(qt)
endif()
