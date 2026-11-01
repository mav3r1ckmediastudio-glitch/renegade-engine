get_filename_component(ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
file(READ "${ROOT}/Tools/night_sky/renegadeCloudFormations.hlsli" formation)
file(READ "${ROOT}/cmake/RenegadeNightSkyOverlay.cmake" overlay)
foreach(token "RenegadeFormationCoverage" "RenegadeNativeFormationGradient"
    "RenegadeWeatherQuantile" "amount>=1" "amount<=0")
    string(FIND "${formation}" "${token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Formation invariant missing: ${token}")
    endif()
endforeach()
foreach(token "if (parameters.layer.padding0 == 1)" "return RenegadeIndependentCumulus("
    "return RenegadeNativeFormationGradient(" "return weatherData.r > 0;" "layerFirst.padding0 = 0;")
    string(FIND "${overlay}" "${token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Shared renderer wiring missing: ${token}")
    endif()
endforeach()
message(STATUS "Formation shared 3D density and independent distribution contract passed")

file(READ "${ROOT}/cmake/RenegadeCloudQualityOverlay.cmake" quality)
foreach(token "cloudAllocation" "volumetriccloudResources.final_resolution = internalResolution"
    "res.texture_cloudRender.desc.width" "res.texture_reproject[0].desc.width"
    "uv * postprocess.params0.xy" "ceil((tMax - tMin) / 48.0)" "float emptyStep =")
    string(FIND "${quality}" "${token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Formation reconstruction contract missing: ${token}")
    endif()
endforeach()
