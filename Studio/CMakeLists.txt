add_library(RenegadeTestLevelRuntimeProcess STATIC
    src/TestLevelRuntimeProcess.cpp
    src/TestLevelRuntimeProcess.h
)

add_library(
    Renegade::TestLevelRuntimeProcess
    ALIAS RenegadeTestLevelRuntimeProcess
)

target_include_directories(RenegadeTestLevelRuntimeProcess
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_link_libraries(RenegadeTestLevelRuntimeProcess
    PUBLIC
        Renegade::EngineBridge
)

target_compile_definitions(RenegadeTestLevelRuntimeProcess
    PRIVATE
        UNICODE
        _UNICODE
)

target_compile_options(RenegadeTestLevelRuntimeProcess
    PRIVATE
        "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>"
)

find_package(Git REQUIRED)
execute_process(
    COMMAND "${GIT_EXECUTABLE}" rev-parse HEAD
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    RESULT_VARIABLE RENEGADE_REVISION_RESULT
    OUTPUT_VARIABLE RENEGADE_SOURCE_REVISION
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
execute_process(
    COMMAND "${GIT_EXECUTABLE}" rev-parse HEAD
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/WickedEngine"
    RESULT_VARIABLE RENEGADE_WICKED_REVISION_RESULT
    OUTPUT_VARIABLE RENEGADE_WICKED_REVISION
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
string(LENGTH "${RENEGADE_SOURCE_REVISION}" RENEGADE_SOURCE_REVISION_LENGTH)
string(LENGTH "${RENEGADE_WICKED_REVISION}" RENEGADE_WICKED_REVISION_LENGTH)
if(NOT RENEGADE_REVISION_RESULT EQUAL 0 OR
   NOT RENEGADE_SOURCE_REVISION_LENGTH EQUAL 40 OR
   NOT RENEGADE_SOURCE_REVISION MATCHES "^[0-9a-f]+$")
    message(FATAL_ERROR "Renegade Studio requires an exact Git source revision for LP06 builds.")
endif()
if(NOT RENEGADE_WICKED_REVISION_RESULT EQUAL 0 OR
   NOT RENEGADE_WICKED_REVISION_LENGTH EQUAL 40 OR
   NOT RENEGADE_WICKED_REVISION MATCHES "^[0-9a-f]+$")
    message(FATAL_ERROR "Renegade Studio requires the exact pinned Wicked revision for LP06 builds.")
endif()

add_executable(RenegadeStudio WIN32
    "${CMAKE_SOURCE_DIR}/WickedEngine/Editor/DummyVisualizer.cpp"
    "${CMAKE_SOURCE_DIR}/WickedEngine/Editor/Translator.cpp"
    src/CreatorAssetDragPreview.cpp
    src/RenegadeStudioChrome.cpp
    src/RenegadeStudioChromeAssets.cpp
    src/RenegadeStudioChrome.h
    src/RenegadeStoryFlowWorkspace.cpp
    src/RenegadeStoryFlowWorkspace.h
    src/RenegadeProjectHub.cpp
    src/RenegadeProjectHub.h
    src/RenegadeProjectLoadingOverlay.cpp
    src/RenegadeProjectLoadingOverlay.h
    src/StartupIdentityHandshake.h
    src/StartupIrisTransition.h
    src/StartupIdentityPrompt.cpp
    src/StartupIdentityPrompt.h
    src/StartupMediaFoundationPlayer.cpp
    src/StartupMediaFoundationPlayer.h
    src/StudioApplication.cpp
    src/StudioApplication.h
    src/StudioUserPreferences.cpp
    src/StudioUserPreferences.h
    src/WindowsGameBuildController.cpp
    src/WindowsGameBuildController.h
    src/main_Windows.cpp
)

add_executable(RenegadeTerrainSurfacePacker
    "${CMAKE_SOURCE_DIR}/Tools/TerrainSurfacePacker.cpp"
)
set_target_properties(RenegadeTerrainSurfacePacker PROPERTIES
    FOLDER "Renegade/Tools"
)
add_dependencies(RenegadeStudio RenegadeTerrainSurfacePacker)

target_include_directories(RenegadeStudio
    PRIVATE
        "${CMAKE_SOURCE_DIR}/WickedEngine/Editor"
)

target_link_libraries(RenegadeStudio
    PRIVATE
        Renegade::EngineBridge
        Renegade::ScreenRenderer
        Renegade::TestLevelRuntimeProcess
        mfplat
        mf
        mfuuid
        evr
        strmiids
        ole32
)

target_compile_definitions(RenegadeStudio
    PRIVATE
        UNICODE
        _UNICODE
        RENEGADE_SOURCE_REVISION="${RENEGADE_SOURCE_REVISION}"
        RENEGADE_WICKED_REVISION="${RENEGADE_WICKED_REVISION}"
)

target_compile_options(RenegadeStudio
    PRIVATE
        "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>"
        "$<$<CXX_COMPILER_ID:MSVC>:/bigobj>"
)

set_target_properties(RenegadeStudio PROPERTIES
    FOLDER "Renegade"
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:RenegadeStudio>"
)

# Renegade-owned shaders are compiled at runtime from source shipped beside the
# executable, so they must land under Content/ where Build-Studio-Windows.ps1
# already packages the whole directory. They are deliberately NOT added to
# Wicked's shader dump: they belong to Studio, not to the engine.
add_custom_command(
    TARGET RenegadeStudio POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:RenegadeStudio>/Content"
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:RenegadeStudio>/Content/shaders"
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:RenegadeStudio>/Content/ui"
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:RenegadeStudio>/Content/terrain/default_grass"
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:RenegadeStudio>/BuildInputs"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/WickedEngine/WickedEngine/dxcompiler.dll"
        "$<TARGET_FILE_DIR:RenegadeStudio>"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/WickedEngine/Content/models/cube.wiscene"
        "$<TARGET_FILE_DIR:RenegadeStudio>/Content/cube.wiscene"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders/RenegadeGridVS.hlsl"
        "$<TARGET_FILE_DIR:RenegadeStudio>/Content/shaders/RenegadeGridVS.hlsl"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders/RenegadeGridPS.hlsl"
        "$<TARGET_FILE_DIR:RenegadeStudio>/Content/shaders/RenegadeGridPS.hlsl"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/renegade-engine-wordmark.png"
        "$<TARGET_FILE_DIR:RenegadeStudio>/Content/ui/renegade-engine-wordmark.png"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/creator-human-reference.png"
        "$<TARGET_FILE_DIR:RenegadeStudio>/Content/ui/creator-human-reference.png"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/package/BuildInputs"
        "$<TARGET_FILE_DIR:RenegadeStudio>/BuildInputs"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/WickedEngine/LICENSE.txt"
        "$<TARGET_FILE_DIR:RenegadeStudio>/BuildInputs/WickedEngine-LICENSE.txt"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/WickedEngine/third_party_software.txt"
        "$<TARGET_FILE_DIR:RenegadeStudio>/BuildInputs/WickedEngine-third_party_software.txt"
    COMMAND $<TARGET_FILE:RenegadeTerrainSurfacePacker>
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/terrain/default_grass/default_grass_ao.tga"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/terrain/default_grass/default_grass_roughness.tga"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/terrain/default_grass/default_grass_basecolor.tga"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/terrain/default_grass/default_grass_normal.tga"
        "$<TARGET_FILE_DIR:RenegadeStudio>/Content/terrain/default_grass/default_grass_basecolor.tga"
        "$<TARGET_FILE_DIR:RenegadeStudio>/Content/terrain/default_grass/default_grass_normal.tga"
        "$<TARGET_FILE_DIR:RenegadeStudio>/Content/terrain/default_grass/default_grass_surface.tga"
        "32"
    VERBATIM
)

# Gate 3 concept plate is owner media. CI can omit it; owner packaging injects it.
set(RENEGADE_PROJECT_HUB_CONCEPT
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/renegade-project-hub-concept.jpg")
if(EXISTS "${RENEGADE_PROJECT_HUB_CONCEPT}")
    add_custom_command(TARGET RenegadeStudio POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:RenegadeStudio>/Content/ui"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${RENEGADE_PROJECT_HUB_CONCEPT}"
            "$<TARGET_FILE_DIR:RenegadeStudio>/Content/ui/renegade-project-hub-concept.jpg"
        VERBATIM)
endif()

# Gate 2A plays the owner's original MP4 directly through Windows Media
# Foundation. There is no companion WAV and no Wicked video-decoder dependency.
set(RENEGADE_STARTUP_REVEAL_VIDEO
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/startup/renegade_logo_reveal_v2.mp4")
if(EXISTS "${RENEGADE_STARTUP_REVEAL_VIDEO}")
    add_custom_command(
        TARGET RenegadeStudio POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:RenegadeStudio>/Content/startup"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${RENEGADE_STARTUP_REVEAL_VIDEO}"
            "$<TARGET_FILE_DIR:RenegadeStudio>/Content/startup/renegade_logo_reveal_v2.mp4"
        VERBATIM
    )
else()
    message(STATUS
        "Gate 2A startup MP4 is not present in this checkout; runtime will fail open and owner test packaging must inject the approved MP4 under Content/startup/.")
endif()

# Gate 2C owns both the generated motion plate and an exact final-frame bitmap.
# The motion plate supplies machinery/head/scan animation only. Renegade draws
# the dynamic developer identity, accepted state and ENTER HUB interaction
# natively over the final bitmap so generated pseudo-text is never authoritative.
set(RENEGADE_IDENTITY_HANDSHAKE_VIDEO
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/startup/renegade_identity_handshake_v1.mp4")
set(RENEGADE_IDENTITY_HANDSHAKE_FINAL_FRAME
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/startup/renegade_identity_handshake_final.bmp")
if(EXISTS "${RENEGADE_IDENTITY_HANDSHAKE_VIDEO}" AND
   EXISTS "${RENEGADE_IDENTITY_HANDSHAKE_FINAL_FRAME}")
    add_custom_command(
        TARGET RenegadeStudio POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:RenegadeStudio>/Content/startup"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${RENEGADE_IDENTITY_HANDSHAKE_VIDEO}"
            "$<TARGET_FILE_DIR:RenegadeStudio>/Content/startup/renegade_identity_handshake_v1.mp4"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${RENEGADE_IDENTITY_HANDSHAKE_FINAL_FRAME}"
            "$<TARGET_FILE_DIR:RenegadeStudio>/Content/startup/renegade_identity_handshake_final.bmp"
        VERBATIM
    )
else()
    message(STATUS
        "Gate 2C identity-handshake media is not present in this checkout; runtime will fail soft and owner test packaging must inject both accepted assets under Content/startup/.")
endif()
