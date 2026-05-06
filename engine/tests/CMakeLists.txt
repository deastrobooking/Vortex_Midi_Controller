cmake_minimum_required(VERSION 3.20)

# Google Test via FetchContent
include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# ─── Test executables ──────────────────────────────────────────────────────
macro(add_engine_test name)
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE vortex_lib GTest::gtest_main)
    add_test(NAME ${name} COMMAND ${name})
endmacro()

add_engine_test(test_clock_engine)
add_engine_test(test_automation)
add_engine_test(test_snapshot)
add_engine_test(test_midi_router)
add_engine_test(test_scale_engine)
add_engine_test(test_lfo_shape)
add_engine_test(test_sequencer)
add_engine_test(test_mod_matrix)
