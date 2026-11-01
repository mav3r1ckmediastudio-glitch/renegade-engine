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


# Moon v1 appends two registers; all pre-existing weather offsets remain unchanged.
# Source-of-truth parameters live in Renegade metadata, never upstream serialization.
configure_file("${CMAKE_SOURCE_DIR}/Tools/night_sky/renegadeMoon.hlsli" "${_ns_wicked}/shaders/renegadeMoon.hlsli" COPYONLY)
_ns_replace("shaders/ShaderInterop_Weather.h"
    "VolumetricCloudParameters volumetric_clouds;"
    "VolumetricCloudParameters volumetric_clouds;\n    float4 moon_parameters;\n    float4 moon_orbit;"
    "float4 moon_parameters;")
_ns_replace("wiScene_Components.h"
    "float aurora_coverage = 0.4f;"
    "float aurora_coverage = 0.4f;\n        XMFLOAT4 moon_parameters = XMFLOAT4(0.5f,1.0f,0,0);\n        XMFLOAT4 moon_orbit = XMFLOAT4(0,35,1,0);"
    "XMFLOAT4 moon_parameters =")
_ns_replace("wiScene.cpp"
    "shaderscene.weather.night_padding = XMFLOAT3(0,0,0);"
    "shaderscene.weather.night_padding = XMFLOAT3(0,0,0);\n        shaderscene.weather.moon_parameters = weather.moon_parameters;\n        shaderscene.weather.moon_orbit = weather.moon_orbit;"
    "shaderscene.weather.moon_parameters =")
_ns_replace("shaders/skyHF.hlsli"
    "totalColor += RenegadeNightGradient("
    "float4 moon = RenegadeMoon(worldDirection, sunDirection, GetWeather().moon_parameters, GetWeather().moon_orbit);\n        totalColor = totalColor * (1.0f - moon.a) + moon.rgb;\n        totalColor += RenegadeNightGradient("
    "float4 moon = RenegadeMoon(")

# Meteors reuse four reserved Night Sky floats; GPU structure offsets/sizes do not change.
configure_file("${CMAKE_SOURCE_DIR}/Tools/night_sky/renegadeShootingStars.hlsli" "${_ns_wicked}/shaders/renegadeShootingStars.hlsli" COPYONLY)
_ns_replace("wiScene_Components.h"
    "XMFLOAT4 moon_orbit = XMFLOAT4(0,35,1,0);"
    "XMFLOAT4 moon_orbit = XMFLOAT4(0,35,1,0);\n        XMFLOAT3 shooting_stars = XMFLOAT3(0,1,8);\n        float shooting_star_preview_time = -1000;"
    "XMFLOAT3 shooting_stars =")
_ns_replace("wiScene.cpp"
    "shaderscene.weather.moon_orbit = weather.moon_orbit;"
    "shaderscene.weather.moon_orbit = weather.moon_orbit;\n        shaderscene.weather.night_padding = weather.shooting_stars;\n        shaderscene.weather.night_alignment_padding = weather.shooting_star_preview_time;"
    "shaderscene.weather.night_padding = weather.shooting_stars;")
_ns_replace("shaders/skyHF.hlsli"
    "if (GetFrame().options & OPTION_BIT_HEIGHT_FOG)"
    "if (!darkMode) totalColor += RenegadeShootingStars(worldDirection, sunDirection, GetWeather().night_padding, GetTime(), GetWeather().night_alignment_padding);\n    if (GetFrame().options & OPTION_BIT_HEIGHT_FOG)"
    "totalColor += RenegadeShootingStars(")

# Moonlit Clouds V1: reuse the existing cloud raymarch and its directional
# self-shadowing rather than running a second light-volume integration.
configure_file("${CMAKE_SOURCE_DIR}/Tools/night_sky/renegadeMoonCloudLighting.hlsli"
    "${_ns_wicked}/shaders/renegadeMoonCloudLighting.hlsli" COPYONLY)
_ns_replace("shaders/volumetricCloud_renderCS.hlsl"
    "#include \"volumetricCloudsHF.hlsli\""
    "#include \"volumetricCloudsHF.hlsli\"\n#include \"renegadeMoonCloudLighting.hlsli\""
    "#include \"renegadeMoonCloudLighting.hlsli\"")
_ns_replace("shaders/volumetricCloud_renderCS.hlsl"
    "float3 sunDirection = GetSunDirection();"
    "float3 sunDirection = GetSunDirection();\n    RenegadeApplyMoonlightToClouds(sunDirection, sunIlluminance, GetWeather().moon_parameters, GetWeather().moon_orbit);"
    "RenegadeApplyMoonlightToClouds(")

# Keep native atmospheric sun illumination untouched: the same cloud shader
# also computes aerial perspective after the cloud raymarch.
_ns_replace("shaders/volumetricCloud_renderCS.hlsl"
    "RenegadeApplyMoonlightToClouds(sunDirection, sunIlluminance, GetWeather().moon_parameters, GetWeather().moon_orbit);"
    "float3 cloudLightDirection = sunDirection;\n    float3 cloudLightIlluminance = sunIlluminance;\n    RenegadeApplyMoonlightToClouds(cloudLightDirection, cloudLightIlluminance, GetWeather().moon_parameters, GetWeather().moon_orbit);"
    "float3 cloudLightDirection = sunDirection;")
_ns_replace("shaders/volumetricCloud_renderCS.hlsl"
    "float cosTheta = dot(rayDirection, sunDirection);"
    "float cosTheta = dot(rayDirection, cloudLightDirection);"
    "float cosTheta = dot(rayDirection, cloudLightDirection);")
_ns_replace("shaders/volumetricCloud_renderCS.hlsl"
    "VolumetricCloudLighting(atmosphere, rayOrigin, sampleWorldPosition, sunDirection, sunIlluminance, cosTheta"
    "VolumetricCloudLighting(atmosphere, rayOrigin, sampleWorldPosition, cloudLightDirection, cloudLightIlluminance, cosTheta"
    "cloudLightDirection, cloudLightIlluminance, cosTheta")

# Gate 1 core-patch decision: reuse reserved layer padding0 as a transient
# formation ID (0=legacy). No GPU layout or native archive change. Metadata owns
# serialization. Initialise both registers; all CPU/HLSL targets rebuild together.
configure_file("${CMAKE_SOURCE_DIR}/Tools/night_sky/renegadeCloudFormations.hlsli"
    "${_ns_wicked}/shaders/renegadeCloudFormations.hlsli" COPYONLY)
_ns_replace("shaders/ShaderInterop_Weather.h"
    "// First\n"
    "// First\n        layerFirst.padding0 = 0;\n        layerSecond.padding0 = 0;\n"
    "layerFirst.padding0 = 0;")
_ns_replace("shaders/volumetricCloudsHF.hlsli"
    "#define HALF_FLT_MAX 65504.0"
    "#define HALF_FLT_MAX 65504.0\n#include \"renegadeCloudFormations.hlsli\""
    "#include \"renegadeCloudFormations.hlsli\"")
_ns_replace("shaders/volumetricCloudsHF.hlsli"
    "float3 weatherData = texture_weatherMap.SampleLevel"
    "if (parameters.layer.padding0 >= 1 && parameters.layer.padding0 <= 5)\n        return float3(RenegadeFormationCoverage(pos, heightFraction, parameters.layer.coverageAmount, parameters.layer.padding0), texture_weatherMap.SampleLevel(sampler_linear_wrap, pos * parameters.layer.weatherScale * 4.0, 0).g, 0);\n\n    float3 weatherData = texture_weatherMap.SampleLevel"
    "RenegadeFormationCoverage(pos,")
_ns_replace("shaders/volumetricCloudsHF.hlsli"
    "bool ValidCloudDensity(float heightFraction, float3 weatherData, LayerParameters parameters)\n{"
    "bool ValidCloudDensity(float heightFraction, float3 weatherData, LayerParameters parameters)\n{\n    if (parameters.layer.padding0 >= 1 && parameters.layer.padding0 <= 5) return weatherData.r > 0;"
    "return weatherData.r > 0;")
# Independent Cumulus volume experiment; all other profiles and Legacy use
# the native kernel while this bounded morphology change is evaluated.
_ns_replace("shaders/volumetricCloudsHF.hlsli"
"// Renegade formations retain the pinned native 3D density kernel."
[=[// Renegade independent Cumulus experiment.
    if (parameters.layer.padding0 == 1)
        return RenegadeIndependentCumulus(texture_shapeNoise, texture_detailNoise,
            p + float3(parameters.windOffset.x, 0, parameters.windOffset.z),
            heightFraction, weatherData.rg, lod, sampleDetail, parameters.windOffset.y);]=]
"// Renegade independent Cumulus experiment.")
_ns_replace("shaders/volumetricCloudsHF.hlsli"
"return (parameters.layer.gradientSmall * weatherTypeMask.r)"
"if (parameters.layer.padding0 >= 1 && parameters.layer.padding0 <= 5) return RenegadeNativeFormationGradient(parameters.layer.padding0, weatherTypeMask);\n    return (parameters.layer.gradientSmall * weatherTypeMask.r)"
"return RenegadeNativeFormationGradient(")
_ns_replace("shaders/volumetricCloudsHF.hlsli"
"return (parameters.layer.anvilDeformationSmall * weatherTypeMask.r)"
"if (parameters.layer.padding0 >= 1 && parameters.layer.padding0 <= 5) return RenegadeNativeFormationAnvil(parameters.layer.padding0);\n    return (parameters.layer.anvilDeformationSmall * weatherTypeMask.r)"
"return RenegadeNativeFormationAnvil(")

# Update the first local formation prototype in already configured worktrees.
_ns_replace("shaders/volumetricCloudsHF.hlsli"
    "RenegadeFormationCoverage(pos, parameters.layer.coverageAmount, parameters.layer.padding0)"
    "RenegadeFormationCoverage(pos, heightFraction, parameters.layer.coverageAmount, parameters.layer.padding0)"
    "RenegadeFormationCoverage(pos, heightFraction,")

# High Cirrus: intersect its real altitude band, preserve native low/legacy paths.
_ns_replace("shaders/volumetricCloud_renderCS.hlsl"
    "const float cloudBottomRadius = planetRadius + GetWeather().volumetric_clouds.cloudStartHeight;"
    "const float cloudBottomRadius = RenegadeCloudMarchBottomRadius(planetRadius + GetWeather().volumetric_clouds.cloudStartHeight);"
    "const float cloudBottomRadius = RenegadeCloudMarchBottomRadius(")
_ns_replace("shaders/volumetricCloud_renderCS.hlsl"
    "tMin > GetWeather().volumetric_clouds.renderDistance"
    "tMin > RenegadeCloudMarchRenderDistance(planetRadius, cloudTopRadius, GetWeather().volumetric_clouds.renderDistance)"
    "tMin > RenegadeCloudMarchRenderDistance(")
_ns_replace("shaders/volumetricCloud_renderCS.hlsl"
    "float fogDistance = saturate(tDepth * GetWeather().volumetric_clouds.horizonBlendAmount);"
    "float fogDistance = saturate(RenegadeCloudHorizonDistance(tDepth) * GetWeather().volumetric_clouds.horizonBlendAmount);"
    "saturate(RenegadeCloudHorizonDistance(tDepth)")

include("${CMAKE_SOURCE_DIR}/cmake/RenegadeCloudQualityOverlay.cmake")

# Migrate the fixed-profile control without changing the native density kernel.
_ns_replace("shaders/volumetricCloudsHF.hlsli"
"return RenegadeNativeFormationGradient(parameters.layer.padding0);"
"return RenegadeNativeFormationGradient(parameters.layer.padding0, weatherTypeMask);"
"return RenegadeNativeFormationGradient(parameters.layer.padding0, weatherTypeMask);")
_ns_replace("shaders/volumetricCloudsHF.hlsli"
"return float3(RenegadeFormationCoverage(pos, heightFraction, parameters.layer.coverageAmount, parameters.layer.padding0), .5, 0);"
"return float3(RenegadeFormationCoverage(pos, heightFraction, parameters.layer.coverageAmount, parameters.layer.padding0), texture_weatherMap.SampleLevel(sampler_linear_wrap, pos * parameters.layer.weatherScale * 4.0, 0).g, 0);"
"pos * parameters.layer.weatherScale * 4.0")


# Gate 1 macro-volume experiment: alpha was unused by the native density path.
# Preserve every native RGB value and reuse its resource/bindings/mip handling.
_ns_replace("shaders/volumetricCloud_detailnoiseCS.hlsl"
"output[DTid] = float4(worleyLarge, worleyMedium, worleySmall, 1.0);"
"output[DTid] = float4(worleyLarge, worleyMedium, worleySmall, saturate(Voronoi(pos, 4.0, 13.0, true).r));"
"saturate(Voronoi(pos, 4.0, 13.0, true).r)")

include("${CMAKE_SOURCE_DIR}/cmake/RenegadeCloudBodyCacheOverlay.cmake")
