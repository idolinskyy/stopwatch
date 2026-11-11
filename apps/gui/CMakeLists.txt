find_package(Qt6 REQUIRED COMPONENTS Widgets)

qt_add_executable(gui_app
    src/main.cpp
    src/main_window.hpp
    src/main_window.cpp
    src/digital_board_widget.hpp
    src/digital_board_widget.cpp
)

qt_add_resources(gui_app "gui_assets"
    PREFIX "/sw"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}/assets"
    FILES assets/chevron-down.png
)

set_target_properties(gui_app PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
    AUTORCC ON
    WIN32_EXECUTABLE ON
    MACOSX_BUNDLE ON
)

target_link_libraries(gui_app PRIVATE
    StopWatch::Core
    Qt6::Widgets
    Project::Options
    Project::Warnings
    Project::StrictWarnings
)
set_target_properties(gui_app PROPERTIES OUTPUT_NAME "StopWatch-gui")
target_compile_definitions(gui_app PRIVATE STOPWATCH_VERSION="${PROJECT_VERSION}")

# RPATH на каталог Qt. Порожній елемент, який додає збірка Qt, ламає cmake --install.
if(UNIX AND NOT APPLE)
    get_target_property(_qt_core_lib Qt6::Core IMPORTED_LOCATION)
    if(NOT _qt_core_lib)
        get_target_property(_qt_core_lib Qt6::Core IMPORTED_LOCATION_RELEASE)
    endif()
    if(NOT _qt_core_lib)
        get_target_property(_qt_core_lib Qt6::Core IMPORTED_LOCATION_DEBUG)
    endif()
    if(_qt_core_lib)
        cmake_path(GET _qt_core_lib PARENT_PATH _qt_lib_dir)
        set_target_properties(gui_app PROPERTIES
            BUILD_RPATH "${_qt_lib_dir}"
            INSTALL_RPATH "${_qt_lib_dir}"
            INSTALL_RPATH_USE_LINK_PATH FALSE)
    endif()
endif()
