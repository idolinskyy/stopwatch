add_executable(cli_app src/main.cpp)

target_link_libraries(cli_app PRIVATE StopWatch::Core Project::Options Project::Warnings Project::StrictWarnings)

set_target_properties(cli_app PROPERTIES OUTPUT_NAME "StopWatch")
