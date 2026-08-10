add_library(RenegadeRuntimeBootstrap STATIC
    src/RuntimeActions.cpp
    src/RuntimeActions.h
    src/RuntimeBootstrap.cpp
    src/RuntimeBootstrap.h
    src/RuntimeFlow.cpp
    src/RuntimeFlow.h
    src/RuntimeScreen.cpp
    src/RuntimeScreen.h
)

add_library(Renegade::RuntimeBootstrap ALIAS RenegadeRuntimeBootstrap)

target_link_libraries(RenegadeRuntimeBootstrap
    PUBLIC
        Renegade::EngineBridge
)

target_include_directories(RenegadeRuntimeBootstrap
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_options(RenegadeRuntimeBootstrap
    PRIVATE
        "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>"
)

set_target_properties(RenegadeRuntimeBootstrap PROPERTIES
    FOLDER "Renegade"
)

add_executable(RenegadeRuntime WIN32
    src/RuntimeApplication.cpp
    src/RuntimeApplication.h
    src/RuntimeSmokeDiagnostics.cpp
    src/main_Windows.cpp
)
add_dependencies(RenegadeRuntime RenegadeTerrainSurfacePacker)

target_link_libraries(RenegadeRuntime
    PRIVATE
        Renegade::RuntimeBootstrap
)

target_compile_definitions(RenegadeRuntime
    PRIVATE
        UNICODE
        _UNICODE
)

target_compile_options(RenegadeRuntime
    PRIVATE
        "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>"
)

set_target_properties(RenegadeRuntime PROPERTIES
    FOLDER "Renegade"
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:RenegadeRuntime>"
)

add_custom_command(
    TARGET RenegadeRuntime POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:RenegadeRuntime>/Content"
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:RenegadeRuntime>/Content/terrain/default_grass"
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:RenegadeRuntime>/Content/LP01/Valid Project/Content/Scenes"
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:RenegadeRuntime>/Content/LP01/Invalid Version"
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:RenegadeRuntime>/Content/LP01/Escaping Path"
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:RenegadeRuntime>/Content/LP01/Missing Scene"
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:RenegadeRuntime>/Content/LP01/Corrupt Scene/Content/Scenes"
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:RenegadeRuntime>/Content/LP02/Valid Flow/Content/Flow"
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:RenegadeRuntime>/Content/LP02/Valid Flow/Content/Scenes"
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:RenegadeRuntime>/Content/LP03/Valid Screen/Content/Flow"
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:RenegadeRuntime>/Content/LP03/Valid Screen/Content/Scenes"
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:RenegadeRuntime>/Content/LP03/Valid Screen/Content/UI"
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:RenegadeRuntime>/Content/LP03/Invalid Screens"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/WickedEngine/WickedEngine/dxcompiler.dll"
        "$<TARGET_FILE_DIR:RenegadeRuntime>"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/WickedEngine/Content/models/cube.wiscene"
        "$<TARGET_FILE_DIR:RenegadeRuntime>/Content/cube.wiscene"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/WickedEngine/Content/models/cube.wiscene"
        "$<TARGET_FILE_DIR:RenegadeRuntime>/Content/LP01/Valid Project/Content/Scenes/BootstrapTest.wiscene"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/LP01/BootstrapProject.renegade"
        "$<TARGET_FILE_DIR:RenegadeRuntime>/Content/LP01/Valid Project/BootstrapProject.renegade"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/LP01/InvalidVersion.renegade"
        "$<TARGET_FILE_DIR:RenegadeRuntime>/Content/LP01/Invalid Version/InvalidVersion.renegade"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/LP01/EscapingPath.renegade"
        "$<TARGET_FILE_DIR:RenegadeRuntime>/Content/LP01/Escaping Path/EscapingPath.renegade"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/LP01/MissingScene.renegade"
        "$<TARGET_FILE_DIR:RenegadeRuntime>/Content/LP01/Missing Scene/MissingScene.renegade"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/LP01/CorruptScene.renegade"
        "$<TARGET_FILE_DIR:RenegadeRuntime>/Content/LP01/Corrupt Scene/CorruptScene.renegade"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/LP01/Corrupt.wiscene"
        "$<TARGET_FILE_DIR:RenegadeRuntime>/Content/LP01/Corrupt Scene/Content/Scenes/Corrupt.wiscene"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/LP02/Valid Flow/FlowProject.renegade"
        "$<TARGET_FILE_DIR:RenegadeRuntime>/Content/LP02/Valid Flow/FlowProject.renegade"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/LP02/Valid Flow/Content/Flow/Main.renegade-flow"
        "$<TARGET_FILE_DIR:RenegadeRuntime>/Content/LP02/Valid Flow/Content/Flow/Main.renegade-flow"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/WickedEngine/Content/models/cube.wiscene"
        "$<TARGET_FILE_DIR:RenegadeRuntime>/Content/LP02/Valid Flow/Content/Scenes/LevelOne.wiscene"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/WickedEngine/Content/models/cube.wiscene"
        "$<TARGET_FILE_DIR:RenegadeRuntime>/Content/LP02/Valid Flow/Content/Scenes/LevelTwo.wiscene"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/LP02/Valid Flow/Content/Scenes/LevelOne.wiscene.rmeta"
        "$<TARGET_FILE_DIR:RenegadeRuntime>/Content/LP02/Valid Flow/Content/Scenes/LevelOne.wiscene.rmeta"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/LP02/Valid Flow/Content/Scenes/LevelTwo.wiscene.rmeta"
        "$<TARGET_FILE_DIR:RenegadeRuntime>/Content/LP02/Valid Flow/Content/Scenes/LevelTwo.wiscene.rmeta"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/LP03/Valid Screen/ScreenProject.renegade"
        "$<TARGET_FILE_DIR:RenegadeRuntime>/Content/LP03/Valid Screen/ScreenProject.renegade"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/LP03/Valid Screen/Content/Flow/Main.renegade-flow"
        "$<TARGET_FILE_DIR:RenegadeRuntime>/Content/LP03/Valid Screen/Content/Flow/Main.renegade-flow"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/LP03/Valid Screen/Content/UI/Main.renegade-screen"
        "$<TARGET_FILE_DIR:RenegadeRuntime>/Content/LP03/Valid Screen/Content/UI/Main.renegade-screen"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/LP03/Valid Screen/Content/UI/renegade-runtime-background.png"
        "$<TARGET_FILE_DIR:RenegadeRuntime>/Content/LP03/Valid Screen/Content/UI/renegade-runtime-background.png"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/WickedEngine/Content/models/cube.wiscene"
        "$<TARGET_FILE_DIR:RenegadeRuntime>/Content/LP03/Valid Screen/Content/Scenes/LevelOne.wiscene"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/WickedEngine/Content/models/cube.wiscene"
        "$<TARGET_FILE_DIR:RenegadeRuntime>/Content/LP03/Valid Screen/Content/Scenes/LevelTwo.wiscene"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/LP03/Valid Screen/Content/Scenes/LevelOne.wiscene.rmeta"
        "$<TARGET_FILE_DIR:RenegadeRuntime>/Content/LP03/Valid Screen/Content/Scenes/LevelOne.wiscene.rmeta"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/LP03/Valid Screen/Content/Scenes/LevelTwo.wiscene.rmeta"
        "$<TARGET_FILE_DIR:RenegadeRuntime>/Content/LP03/Valid Screen/Content/Scenes/LevelTwo.wiscene.rmeta"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/LP03/Invalid Screens"
        "$<TARGET_FILE_DIR:RenegadeRuntime>/Content/LP03/Invalid Screens"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/package/Run-LP03-Screen-Proof.cmd"
        "$<TARGET_FILE_DIR:RenegadeRuntime>/Run-LP03-Screen-Proof.cmd"
    COMMAND $<TARGET_FILE:RenegadeTerrainSurfacePacker>
        "${CMAKE_SOURCE_DIR}/Studio/assets/terrain/default_grass/default_grass_ao.tga"
        "${CMAKE_SOURCE_DIR}/Studio/assets/terrain/default_grass/default_grass_roughness.tga"
        "${CMAKE_SOURCE_DIR}/Studio/assets/terrain/default_grass/default_grass_basecolor.tga"
        "${CMAKE_SOURCE_DIR}/Studio/assets/terrain/default_grass/default_grass_normal.tga"
        "$<TARGET_FILE_DIR:RenegadeRuntime>/Content/terrain/default_grass/default_grass_basecolor.tga"
        "$<TARGET_FILE_DIR:RenegadeRuntime>/Content/terrain/default_grass/default_grass_normal.tga"
        "$<TARGET_FILE_DIR:RenegadeRuntime>/Content/terrain/default_grass/default_grass_surface.tga"
        "32"
    VERBATIM
)
