find_program(RENEGADE_SCREEN_DXC dxc HINTS
    "$ENV{WindowsSdkDir}/bin/${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}/x64"
    "[HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows Kits\\Installed Roots;KitsRoot10]/bin/${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}/x64"
    REQUIRED)
set(screen_matte_header "${CMAKE_CURRENT_BINARY_DIR}/ScreenVideoMatteCS.generated.h")
add_custom_command(OUTPUT "${screen_matte_header}"
    COMMAND "${RENEGADE_SCREEN_DXC}" -T cs_6_0 -E main -O3 -Qstrip_debug
        -Fh "${screen_matte_header}" -Vn kScreenVideoMatte
        "${CMAKE_CURRENT_SOURCE_DIR}/src/ScreenVideoMatteCS.hlsl"
    DEPENDS src/ScreenVideoMatteCS.hlsl VERBATIM)
set(screen_liquid_header "${CMAKE_CURRENT_BINARY_DIR}/ScreenLiquidCS.generated.h")
add_custom_command(OUTPUT "${screen_liquid_header}"
    COMMAND "${RENEGADE_SCREEN_DXC}" -T cs_6_0 -E main -O3 -Qstrip_debug
        -Fh "${screen_liquid_header}" -Vn kScreenLiquid
        "${CMAKE_CURRENT_SOURCE_DIR}/src/ScreenLiquidCS.hlsl"
    DEPENDS src/ScreenLiquidCS.hlsl VERBATIM)
set(screen_minimap_header "${CMAKE_CURRENT_BINARY_DIR}/ScreenMinimapCS.generated.h")
add_custom_command(OUTPUT "${screen_minimap_header}"
    COMMAND "${RENEGADE_SCREEN_DXC}" -T cs_6_0 -E main -O3 -Qstrip_debug
        -Fh "${screen_minimap_header}" -Vn kScreenMinimap
        "${CMAKE_CURRENT_SOURCE_DIR}/src/ScreenMinimapCS.hlsl"
    DEPENDS src/ScreenMinimapCS.hlsl VERBATIM)
add_library(RenegadeScreenRenderer STATIC
    "${screen_minimap_header}"
    "${screen_liquid_header}"
    "${screen_matte_header}"
    src/ScreenVideoAudio.cpp
    src/DialoguePanel.cpp
    include/renegade/screen/ScreenRenderer.h
    src/ScreenRenderer.cpp
)

add_library(Renegade::ScreenRenderer ALIAS RenegadeScreenRenderer)

target_include_directories(RenegadeScreenRenderer
    PRIVATE ${CMAKE_CURRENT_BINARY_DIR}
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(RenegadeScreenRenderer
    PUBLIC
        Renegade::EngineBridge
    PRIVATE mfplat mfuuid shlwapi oleaut32 ole32
)

target_compile_options(RenegadeScreenRenderer
    PRIVATE
        "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>"
)

set_target_properties(RenegadeScreenRenderer PROPERTIES
    FOLDER "Renegade"
)
