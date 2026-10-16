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
message(STATUS "Renegade Night Sky overlay ready (pinned WickedEngine)")
