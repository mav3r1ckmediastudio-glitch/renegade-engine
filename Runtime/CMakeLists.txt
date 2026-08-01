add_executable(RenegadeRuntime WIN32
    src/RuntimeApplication.cpp
    src/RuntimeApplication.h
    src/main_Windows.cpp
)
add_dependencies(RenegadeRuntime RenegadeTerrainSurfacePacker)

target_link_libraries(RenegadeRuntime
    PRIVATE
        Renegade::EngineBridge
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
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/WickedEngine/WickedEngine/dxcompiler.dll"
        "$<TARGET_FILE_DIR:RenegadeRuntime>"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/WickedEngine/Content/models/cube.wiscene"
        "$<TARGET_FILE_DIR:RenegadeRuntime>/Content/cube.wiscene"
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
