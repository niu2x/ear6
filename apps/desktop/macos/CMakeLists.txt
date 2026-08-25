enable_language(OBJCXX)

configure_file(Info.plist.in "${CMAKE_CURRENT_BINARY_DIR}/Info.plist" @ONLY)

add_executable(ear6-desktop MACOSX_BUNDLE main.mm)

target_link_libraries(ear6-desktop PRIVATE
    ear6-desktop-common
    ear6_a
    "-framework AppKit"
    "-framework AudioToolbox"
    "-framework CoreGraphics"
    "-framework UniformTypeIdentifiers"
)

target_compile_options(ear6-desktop PRIVATE -fobjc-arc)
cxx_project_preset(ear6-desktop)
warning_as_error_enable(ear6-desktop)

set_target_properties(ear6-desktop PROPERTIES
    OUTPUT_NAME Ear6
    OBJCXX_STANDARD 20
    OBJCXX_STANDARD_REQUIRED ON
    OBJCXX_EXTENSIONS OFF
    MACOSX_BUNDLE_BUNDLE_NAME Ear6
    MACOSX_BUNDLE_GUI_IDENTIFIER com.ear6.emulator
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_BINARY_DIR}/Info.plist"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
)

install(TARGETS ear6-desktop BUNDLE DESTINATION .)
