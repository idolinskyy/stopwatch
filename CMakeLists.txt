cmake_minimum_required(VERSION 3.25)

project(
    StopWatch
    VERSION 1.0.0
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(BUILD_TESTS "Build unit tests" ON)
option(BUILD_APPS "Build the console application" ON)
option(BUILD_GUI "Build the Qt Widgets application" ON)
option(ENABLE_WARNINGS_AS_ERRORS "Treat project warnings as errors" OFF)
option(ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
option(ENABLE_CLANG_TIDY "Run clang-tidy while compiling project targets" OFF)

add_library(project_options INTERFACE)
add_library(Project::Options ALIAS project_options)

if(ENABLE_SANITIZERS)
    if(WIN32 OR NOT CMAKE_CXX_COMPILER_ID MATCHES "^(GNU|Clang|AppleClang)$")
        message(FATAL_ERROR "ENABLE_SANITIZERS requires GCC or Clang on a Unix-like platform")
    endif()
    target_compile_options(project_options INTERFACE
        -fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all)
    target_link_options(project_options INTERFACE -fsanitize=address,undefined)
endif()

add_library(project_warnings INTERFACE)
add_library(Project::Warnings ALIAS project_warnings)

if(ENABLE_WARNINGS_AS_ERRORS)
    target_compile_options(project_warnings INTERFACE
        $<$<CXX_COMPILER_ID:MSVC>:/WX>
        $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>,$<CXX_COMPILER_ID:GNU>>:-Werror>)
endif()

target_compile_options(project_warnings INTERFACE
    $<$<CXX_COMPILER_ID:MSVC>:
        /W4 /permissive->
    $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>,$<CXX_COMPILER_ID:GNU>>:
        -Wall -Wextra -Wpedantic -Wnon-virtual-dtor>
)

add_library(project_strict_warnings INTERFACE)
add_library(Project::StrictWarnings ALIAS project_strict_warnings)

target_compile_options(project_strict_warnings INTERFACE
    $<$<CXX_COMPILER_ID:MSVC>:
        /w14242 /w14254 /w14265 /w14287 /w14826>
    $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>,$<CXX_COMPILER_ID:GNU>>:
        -Wconversion -Wsign-conversion -Wshadow>
)

add_subdirectory(libs/core)

if(BUILD_APPS)
    add_subdirectory(apps/cli)
endif()

if(BUILD_GUI)
    add_subdirectory(apps/gui)
endif()

if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

include(GNUInstallDirs)

install(FILES LICENSE NOTICE
    DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/doc/stopwatch")

if(TARGET cli_app)
    install(TARGETS cli_app RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

if(TARGET gui_app)
    install(TARGETS gui_app RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

    set(STOPWATCH_GUI_EXEC "${CMAKE_INSTALL_FULL_BINDIR}/StopWatch-gui")
    set(STOPWATCH_ICON "${CMAKE_INSTALL_FULL_DATAROOTDIR}/icons/hicolor/scalable/apps/stopwatch.svg")
    configure_file(
        packaging/stopwatch.desktop.in
        "${CMAKE_CURRENT_BINARY_DIR}/stopwatch.desktop"
        @ONLY)
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/stopwatch.desktop"
        DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/applications")
    install(FILES packaging/stopwatch.svg
        DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/scalable/apps")
endif()

if(ENABLE_CLANG_TIDY)
    find_program(CLANG_TIDY_EXECUTABLE NAMES clang-tidy-22 clang-tidy-21 clang-tidy REQUIRED)
    foreach(project_target IN ITEMS core_lib cli_app gui_app unit_tests)
        if(TARGET ${project_target})
            set_target_properties(${project_target} PROPERTIES
                CXX_CLANG_TIDY "${CLANG_TIDY_EXECUTABLE};--warnings-as-errors=*")
        endif()
    endforeach()
endif()

find_program(CLANG_FORMAT_EXECUTABLE NAMES clang-format-22 clang-format-21 clang-format)
if(CLANG_FORMAT_EXECUTABLE)
    file(GLOB_RECURSE PROJECT_FORMAT_SOURCES CONFIGURE_DEPENDS
        "${PROJECT_SOURCE_DIR}/apps/*.cpp" "${PROJECT_SOURCE_DIR}/apps/*.hpp"
        "${PROJECT_SOURCE_DIR}/libs/*.cpp" "${PROJECT_SOURCE_DIR}/libs/*.hpp"
        "${PROJECT_SOURCE_DIR}/tests/*.cpp" "${PROJECT_SOURCE_DIR}/tests/*.hpp")
    add_custom_target(format
        COMMAND "${CLANG_FORMAT_EXECUTABLE}" -i ${PROJECT_FORMAT_SOURCES}
        VERBATIM)
    add_custom_target(format-check
        COMMAND "${CLANG_FORMAT_EXECUTABLE}" --dry-run --Werror ${PROJECT_FORMAT_SOURCES}
        VERBATIM)
endif()
