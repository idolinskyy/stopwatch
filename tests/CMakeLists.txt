include(FetchContent)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.5.2
    SYSTEM)
FetchContent_MakeAvailable(Catch2)

foreach(catch_target IN ITEMS Catch2 Catch2WithMain)
    if(TARGET ${catch_target})
        target_compile_options(${catch_target} PRIVATE
            $<$<CXX_COMPILER_ID:MSVC>:/w>
            $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>,$<CXX_COMPILER_ID:GNU>>:-w>)
        set_target_properties(${catch_target} PROPERTIES COMPILE_WARNING_AS_ERROR OFF)
    endif()
endforeach()

add_executable(unit_tests
    test_stopwatch.cpp
    test_timer.cpp
)

target_link_libraries(unit_tests PRIVATE
    StopWatch::Core
    Project::Options
    Catch2::Catch2WithMain
    Project::Warnings        # Підключаємо тільки базові, без StrictWarnings
)

include(Catch)
catch_discover_tests(unit_tests)
