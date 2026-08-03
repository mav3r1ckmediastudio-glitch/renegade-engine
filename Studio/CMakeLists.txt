add_executable(RenegadeStudio WIN32
    "${CMAKE_SOURCE_DIR}/WickedEngine/Editor/Translator.cpp"
    src/RenegadeStudioChrome.cpp
    src/RenegadeStudioChrome.h
    src/StudioApplication.cpp
    src/StudioApplication.h
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
)

target_compile_definitions(RenegadeStudio
    PRIVATE
        UNICODE
        _UNICODE
)

target_compile_options(RenegadeStudio
    PRIVATE
        "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>"
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
        "$<TARGET_FILE_DIR:RenegadeStudio>/Content/reference/low_poly_human"
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
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/reference/low_poly_human/scene.gltf"
        "$<TARGET_FILE_DIR:RenegadeStudio>/Content/reference/low_poly_human/scene.gltf"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/reference/low_poly_human/scene.bin"
        "$<TARGET_FILE_DIR:RenegadeStudio>/Content/reference/low_poly_human/scene.bin"
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
