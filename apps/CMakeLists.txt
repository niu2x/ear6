if(NOT CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
    add_subdirectory(cli)
endif()

if(EAR6_BUILD_DESKTOP)
    add_subdirectory(desktop)
endif()

if(EAR6_BUILD_WASM)
    if(NOT CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
        message(FATAL_ERROR "WebAssembly build requires Emscripten.")
    endif()
    add_subdirectory(web/wasm)
endif()
