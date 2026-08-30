cmake_minimum_required(VERSION 3.19)

# CMP0141 controls how MSVC debug information format is set. It must be
# resolved before project() so CMAKE_MSVC_DEBUG_INFORMATION_FORMAT below takes
# effect via the generator instead of the legacy /Zi-in-flags path.
if(POLICY CMP0141)
    cmake_policy(SET CMP0141 NEW)
endif()
set(CMAKE_POLICY_DEFAULT_CMP0141 NEW)

project(RenegadeEngine LANGUAGES C CXX)

# Release /O2 CL.exe has a reproducible access-violation crash
# (-1073741819) inside CloseTypeServerPDB when using the default
# ProgramDatabase (/Zi) debug format under this toolchain. Embedded (/Z7)
# avoids the PDB type-server interaction that triggers it. Debug and
# RelWithDebInfo keep ProgramDatabase since they do not crash and benefit
# from separate .pdb files.
set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT
    "$<IF:$<CONFIG:Release>,Embedded,ProgramDatabase>")

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
include(EngineBridge/JP01PhysicsLua.cmake)
include(EngineBridge/Phase5Gate5.cmake)
include(EngineBridge/Phase5Gate8.cmake)

if(WIN32)
    add_subdirectory(ScreenRenderer)
    add_subdirectory(Studio)
    include(Studio/Phase5Gate8.cmake)
    include(Studio/Phase5Gate9.cmake)
    add_subdirectory(Runtime)
    include(Runtime/LP06Gate3.cmake)
    add_subdirectory(Tests)
    include(Tests/JP01Constraints.cmake)
    include(Tests/JP01CharacterPhysics.cmake)
    include(Tests/JP01VehiclePhysics.cmake)
    include(Tests/JP01RagdollPhysics.cmake)
    include(Tests/JP01SoftBodyPhysics.cmake)
    include(Tests/JP01PhysicsLua.cmake)
    include(Tests/JP01WickedCollider.cmake)
    include(Tests/JP01ReusableAssetPhysics.cmake)
    add_test(
        NAME RenegadeJP01HardeningSourceContract
        COMMAND ${CMAKE_COMMAND}
            -DRENEGADE_SOURCE_DIR=${CMAKE_SOURCE_DIR}
            -P ${CMAKE_SOURCE_DIR}/Tests/JP01HardeningSourceContract.cmake
    )
    include(Tests/Phase5Gate1.cmake)
    include(Tests/Phase5Gate2.cmake)
    include(Tests/Phase5Gate3.cmake)
    include(Tests/Phase5Gate4.cmake)
    include(Tests/Phase5Gate5.cmake)
    include(Tests/Phase5Gate6.cmake)
    include(Tests/Phase5Gate7.cmake)
    include(Tests/Phase5Gate8.cmake)
    include(Tests/SceneUiGate2.cmake)
    include(Tests/SceneUiGate3.cmake)
    include(Tests/SceneUiGate4.cmake)
    include(Tests/SceneUiGate5.cmake)
    include(Tests/SceneUiGate6.cmake)
    include(Tests/StoryFlowGate2.cmake)
    include(Tests/StoryFlowGate3.cmake)
    include(Tests/StoryFlowGate4A.cmake)
    include(Tests/StoryFlowGate4B.cmake)
    include(Tests/StoryFlowGate4C.cmake)
    include(Tests/StoryFlowGate4D.cmake)
    include(Tests/StoryFlowGate5A.cmake)
    include(Tests/StoryFlowGate5B.cmake)
    include(Tests/StoryFlowGate5C.cmake)
    include(Tests/StoryFlowGate5Recovery.cmake)
    include(Tests/StoryFlowGate8E.cmake)
    include(Tests/StoryFlowGate9B.cmake)
    include(Tests/StoryFlowGate9C.cmake)
    include(Tests/StoryFlowGate9D.cmake)
    include(Tests/StoryFlowGate9E.cmake)
    include(Tests/StoryFlowJourneyRecovery9A.cmake)
    include(Tests/LP06Gate1.cmake)
    include(Tests/LP06Gate2.cmake)
    include(Tests/LP06Gate3.cmake)
    include(Tests/LP06Gate4.cmake)
    include(Tests/LP06Gate5.cmake)
    include(Tests/LP07Gate1.cmake)
    include(Tests/LP07Gate2.cmake)
    include(Tests/LP07Gate3.cmake)
    include(Tests/LP07Gate4.cmake)
    include(Tests/LP07Gate5.cmake)
    include(Tests/LP07Gate6.cmake)
    include(Tests/LP08Gate1.cmake)
    include(Tests/LP08Gate2.cmake)
    include(Tests/LP08Gate3.cmake)
    include(Tests/LP08Gate4.cmake)
    include(Tests/LP08Gate5.cmake)
else()
    message(STATUS "Renegade Studio is currently enabled for the Windows-first Phase 2 proof.")
endif()
