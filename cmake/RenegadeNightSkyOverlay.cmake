# Renegade Night Sky integration for the pinned WickedEngine checkout.
# Executed before add_subdirectory(WickedEngine) on this isolated development branch.
# Edits are localized to the checked out submodule; no upstream SHA is modified.
set(_ns_wicked "${CMAKE_SOURCE_DIR}/WickedEngine/WickedEngine")
set(_ns_include "${CMAKE_SOURCE_DIR}/Tools/night_sky/renegadeNightSky.hlsli")
if(NOT EXISTS "${_ns_wicked}/wiScene.cpp")
    message(FATAL_ERROR "Night Sky: initialise the WickedEngine submodule first")
endif()
if(NOT EXISTS "${_ns_include}")
    message(FATAL_ERROR "Night Sky: shader include missing")
endif()
function(_ns_replace relative needle replacement marker)
    set(file "${_ns_wicked}/${relative}")
    file(READ "${file}" original)
    string(FIND "${original}" "${marker}" marker_at)
    if(NOT marker_at EQUAL -1)
        return()
    endif()
    string(FIND "${original}" "${needle}" needle_at)
    if(needle_at EQUAL -1)
        message(FATAL_ERROR "Night Sky overlay anchor missing in ${relative}; WickedEngine revision may have changed")
    endif()
    string(REPLACE "${needle}" "${replacement}" patched "${original}")
    file(WRITE "${file}" "${patched}")
endfunction()
configure_file("${_ns_include}" "${_ns_wicked}/shaders/renegadeNightSky.hlsli" COPYONLY)
configure_file("${CMAKE_SOURCE_DIR}/Tools/night_sky/renegadeNightAtmosphere.hlsli" "${_ns_wicked}/shaders/renegadeNightAtmosphere.hlsli" COPYONLY)
configure_file("${CMAKE_SOURCE_DIR}/Tools/night_sky/renegadeAurora.hlsli" "${_ns_wicked}/shaders/renegadeAurora.hlsli" COPYONLY)
_ns_replace("shaders/ShaderInterop_Weather.h"
    "float3 padding_stars;"
    "float night_sky_gradient;\n    float aurora_intensity;\n    float aurora_speed;"
    "float night_sky_gradient;")
_ns_replace("wiScene_Components.h"
    "float stars = 0.5f;"
    "float stars = 0.5f;\n        // Transient renderer values restored from Renegade Environment metadata.\n        float night_sky_gradient = 0.0f;\n        float aurora_intensity = 0.0f;\n        float aurora_speed = 0.35f;"
    "float night_sky_gradient = 0.0f;")
_ns_replace("wiScene.cpp"
    "shaderscene.weather.stars = weather.stars;"
    "shaderscene.weather.stars = weather.stars;\n        shaderscene.weather.night_sky_gradient = weather.night_sky_gradient;\n        shaderscene.weather.aurora_intensity = weather.aurora_intensity;\n        shaderscene.weather.aurora_speed = weather.aurora_speed;"
    "shaderscene.weather.night_sky_gradient = weather.night_sky_gradient;")
_ns_replace("shaders/skyHF.hlsli"
    "#include \"skyAtmosphere.hlsli\""
    "#include \"skyAtmosphere.hlsli\"\n#include \"renegadeNightSky.hlsli\""
    "#include \"renegadeNightSky.hlsli\"")
_ns_replace("shaders/skyHF.hlsli"
    "if (GetFrame().options & OPTION_BIT_HEIGHT_FOG)"
    "// Renegade sky contributions: off by default, existing sky/stars unchanged.\n    if (!darkMode)\n    {\n        totalColor += RenegadeNightGradient(worldDirection, sunDirection, GetWeather().night_sky_gradient);\n        [branch] if (GetWeather().aurora_intensity > 0.0f)\n        {\n            totalColor += RenegadeAurora(worldDirection, sunDirection, GetWeather().aurora_intensity, GetWeather().aurora_speed, GetTime());\n        }\n    }\n    if (GetFrame().options & OPTION_BIT_HEIGHT_FOG)"
    "totalColor += RenegadeNightGradient(")
_ns_replace("shaders/ShaderInterop_Weather.h"
    "float aurora_speed;"
    "float aurora_speed;\n    float night_alignment_padding;\n    float4 night_zenith;\n    float4 night_horizon;\n    float aurora_coverage;\n    float3 night_padding;"
    "float4 night_zenith;")
_ns_replace("wiScene_Components.h"
    "float aurora_speed = 0.35f;"
    "float aurora_speed = 0.35f;\n        XMFLOAT4 night_zenith = XMFLOAT4(0.0018f,0.003f,0.0085f,0);\n        XMFLOAT4 night_horizon = XMFLOAT4(0.013f,0.012f,0.022f,1.0f);\n        float aurora_coverage = 0.4f;"
    "XMFLOAT4 night_zenith =")
_ns_replace("wiScene.cpp"
    "shaderscene.weather.aurora_speed = weather.aurora_speed;"
    "shaderscene.weather.aurora_speed = weather.aurora_speed;\n        shaderscene.weather.night_alignment_padding = 0.0f;\n        shaderscene.weather.night_zenith = weather.night_zenith;\n        shaderscene.weather.night_horizon = weather.night_horizon;\n        shaderscene.weather.aurora_coverage = weather.aurora_coverage;\n        shaderscene.weather.night_padding = XMFLOAT3(0,0,0);"
    "shaderscene.weather.night_zenith =")
_ns_replace("shaders/skyHF.hlsli"
    "RenegadeNightGradient(worldDirection, sunDirection, GetWeather().night_sky_gradient)"
    "RenegadeNightGradient(worldDirection, sunDirection, GetWeather().night_sky_gradient, GetWeather().night_zenith, GetWeather().night_horizon)"
    "GetWeather().night_zenith, GetWeather().night_horizon)")
_ns_replace("shaders/skyHF.hlsli"
    "GetWeather().aurora_speed, GetTime())"
    "GetWeather().aurora_speed, GetWeather().aurora_coverage, GetTime())"
    "GetWeather().aurora_coverage, GetTime())")
# Migration for worktrees already overlaid by the original prototype.
# Older authoring inserted float4 fields without the CPU/HLSL register pad,
# causing sky_exposure to read as rain_amount (black daytime sky and stars).
_ns_replace("shaders/ShaderInterop_Weather.h"
    "float aurora_speed;\n    float4 night_zenith;"
    "float aurora_speed;\n    float night_alignment_padding;\n    float4 night_zenith;"
    "float night_alignment_padding;")
_ns_replace("wiScene.cpp"
    "shaderscene.weather.aurora_speed = weather.aurora_speed;\n        shaderscene.weather.night_zenith"
    "shaderscene.weather.aurora_speed = weather.aurora_speed;\n        shaderscene.weather.night_alignment_padding = 0.0f;\n        shaderscene.weather.night_zenith"
    "shaderscene.weather.night_alignment_padding = 0.0f;")
configure_file("${CMAKE_SOURCE_DIR}/Tools/night_sky/renegadeMilkyWay.hlsli" "${_ns_wicked}/shaders/renegadeMilkyWay.hlsli" COPYONLY)
# Reserved night_zenith.w is an authored scalar; preserve all GPU ABI offsets.
_ns_replace("shaders/skyHF.hlsli"
    "totalColor += RenegadeNightGradient("
    "totalColor += RenegadeMilkyWay(worldDirection, sunDirection, GetWeather().night_zenith.w);\n        totalColor += RenegadeNightGradient("
    "totalColor += RenegadeMilkyWay(")
message(STATUS "Renegade Night Sky overlay ready (pinned WickedEngine)")
