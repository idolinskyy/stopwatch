add_library(core_lib
    src/time_utils.cpp
    src/stopwatch.cpp
    src/timer.cpp
)

add_library(StopWatch::Core ALIAS core_lib)

target_include_directories(core_lib PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_compile_features(core_lib PUBLIC cxx_std_23)

target_link_libraries(core_lib PRIVATE
    Project::Options
    Project::Warnings
    Project::StrictWarnings
)
