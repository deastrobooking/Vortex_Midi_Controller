cmake_minimum_required(VERSION 3.20)
project(vortex_engine VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ─── Dependencies ─────────────────────────────────────────────────────────────
find_package(PkgConfig REQUIRED)
pkg_check_modules(ALSA   REQUIRED alsa)
pkg_check_modules(LWS    REQUIRED libwebsockets)
pkg_check_modules(SQLITE REQUIRED sqlite3)

# nlohmann/json — header-only, fetched via FetchContent
include(FetchContent)
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)

# ─── Sources ──────────────────────────────────────────────────────────────────
set(ENGINE_SOURCES
    src/core/ClockEngine.cpp
    src/core/MidiRouter.cpp
    src/core/AutomationRecorder.cpp
    src/core/AutomationPlayback.cpp
    src/core/SnapshotEngine.cpp
    src/core/MacroEngine.cpp
    src/midi/MidiOutput.cpp
    src/hardware/ModuleManager.cpp
    src/database/Database.cpp
    src/api/WebSocketServer.cpp
    src/api/MessageHandler.cpp
)

# ─── Library (reused by tests) ────────────────────────────────────────────────
add_library(vortex_lib STATIC ${ENGINE_SOURCES})

target_include_directories(vortex_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${ALSA_INCLUDE_DIRS}
    ${LWS_INCLUDE_DIRS}
    ${SQLITE_INCLUDE_DIRS}
)

target_link_libraries(vortex_lib PUBLIC
    ${ALSA_LIBRARIES}
    ${LWS_LIBRARIES}
    ${SQLITE_LIBRARIES}
    nlohmann_json::nlohmann_json
    pthread
)

target_compile_options(vortex_lib PRIVATE -Wall -Wextra -O2)

# ─── Daemon executable ────────────────────────────────────────────────────────
add_executable(midiworkstationd src/app/main.cpp)
target_link_libraries(midiworkstationd PRIVATE vortex_lib)

install(TARGETS midiworkstationd DESTINATION bin)
install(FILES ${CMAKE_SOURCE_DIR}/../database/schema.sql
        DESTINATION share/vortex)

# ─── Tests ────────────────────────────────────────────────────────────────────
option(BUILD_TESTS "Build unit tests" ON)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
