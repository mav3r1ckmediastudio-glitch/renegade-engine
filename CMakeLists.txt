cmake_minimum_required(VERSION 3.19)

project(RenegadeEngine LANGUAGES C CXX)

include(CTest)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(
    RENEGADE_EMBED_SHADERS
    "Embed Wicked shader binaries in Renegade application targets"
    ON
)

# Renegade owns the application targets. Keep upstream sample/editor targets out
# of this build graph so Wicked remains a pinned rendering/runtime dependency.
set(WICKED_EDITOR OFF CACHE BOOL "" FORCE)
set(WICKED_TESTS OFF CACHE BOOL "" FORCE)
set(WICKED_IMGUI_EXAMPLE OFF CACHE BOOL "" FORCE)
set(WICKED_WINDOWS_TEMPLATE OFF CACHE BOOL "" FORCE)
set(WICKED_LINUX_TEMPLATE OFF CACHE BOOL "" FORCE)
set(WICKED_ENABLE_SYMLINKS OFF CACHE BOOL "" FORCE)
set(WICKED_EMBED_SHADERS ${RENEGADE_EMBED_SHADERS} CACHE BOOL "" FORCE)

add_subdirectory(WickedEngine)
add_subdirectory(EngineBridge)

if(WIN32)
    add_subdirectory(Studio)
    add_subdirectory(Runtime)
    add_subdirectory(Tests)
else()
    message(STATUS "Renegade Studio is currently enabled for the Windows-first Phase 2 proof.")
endif()
