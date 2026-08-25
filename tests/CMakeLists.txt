find_package(GTest QUIET)
set(EAR6_GTEST_LIB GTest::GTest)
set(EAR6_GTEST_MAIN_LIB GTest::Main)
if(NOT GTest_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        googletest
        URL https://codeload.github.com/google/googletest/zip/refs/tags/v1.14.0
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
    set(EAR6_GTEST_LIB gtest)
    set(EAR6_GTEST_MAIN_LIB gtest_main)
endif()

find_package(OpenSSL REQUIRED)
add_executable(ear6-test
    api/lifecycle_test.cpp
    api/availability_test.cpp
    state/state_test.cpp
    systems/nes/mapper_regression_test.cpp
    support/framebuffer_hash.cpp
)
target_compile_definitions(ear6-test PRIVATE EAR6_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_include_directories(ear6-test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/support
)
target_link_libraries(ear6-test PRIVATE
    ear6
    ${EAR6_GTEST_LIB}
    ${EAR6_GTEST_MAIN_LIB}
    OpenSSL::Crypto
)
set_target_properties(ear6-test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)
add_test(NAME ear6-test COMMAND ear6-test)

if(EAR6_BUILD_DESKTOP)
    add_executable(ear6-desktop-common-test
        desktop/common_test.cpp
    )
    target_link_libraries(ear6-desktop-common-test PRIVATE
        ear6-desktop-common
        ear6_a
        ${EAR6_GTEST_LIB}
        ${EAR6_GTEST_MAIN_LIB}
    )
    cxx_project_preset(ear6-desktop-common-test)
    warning_as_error_enable(ear6-desktop-common-test)
    add_test(NAME ear6-desktop-common-test COMMAND ear6-desktop-common-test)
endif()

if(EAR6_BUILD_DESKTOP AND EAR6_BUILD_QT_DESKTOP)
    find_package(Qt6 COMPONENTS Widgets QUIET)
endif()
if(EAR6_BUILD_DESKTOP AND EAR6_BUILD_QT_DESKTOP AND TARGET Qt6::Widgets)
    add_executable(ear6-desktop-qt-test
        ${CMAKE_SOURCE_DIR}/apps/desktop/qt/tests/save_store_test.cpp
        ${CMAKE_SOURCE_DIR}/apps/desktop/qt/save_store.cpp
    )
    target_include_directories(ear6-desktop-qt-test PRIVATE
        ${CMAKE_SOURCE_DIR}/apps/desktop/qt
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_BINARY_DIR}
    )
    target_link_libraries(ear6-desktop-qt-test PRIVATE
        ear6
        ${EAR6_GTEST_LIB}
        Qt6::Widgets
    )
    cxx_project_preset(ear6-desktop-qt-test)
    warning_as_error_enable(ear6-desktop-qt-test)
    add_test(NAME ear6-desktop-qt-test COMMAND ear6-desktop-qt-test)
endif()
