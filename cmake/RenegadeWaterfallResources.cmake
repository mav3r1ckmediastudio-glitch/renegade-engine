# Original Renegade waterfall resources only; no fixture rock/reference art.
find_package(Python3 COMPONENTS Interpreter REQUIRED)
set(RENEGADE_WATERFALL_STAGE "${CMAKE_BINARY_DIR}/waterfall-studio-shaders")
add_custom_target(RenegadeWaterfallShaders
 COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/Tools/prepare_placeable_waterfall_shaders.py" "${RENEGADE_WATERFALL_STAGE}"
 COMMAND $<TARGET_FILE:RenegadeRiverShaderCompiler> "${RENEGADE_WATERFALL_STAGE}/PlaceableWaterfallPS.hlsl" "${RENEGADE_WATERFALL_STAGE}" "${RENEGADE_WATERFALL_STAGE}/PlaceableWaterfallPS.cso" ps
 COMMAND $<TARGET_FILE:RenegadeRiverShaderCompiler> "${RENEGADE_WATERFALL_STAGE}/PlaceableWaterfallVS.hlsl" "${RENEGADE_WATERFALL_STAGE}" "${RENEGADE_WATERFALL_STAGE}/PlaceableWaterfallVS.cso" vs
 VERBATIM)
function(renegade_deploy_waterfall_resources target)
 add_dependencies(${target} RenegadeWaterfallShaders)
 add_custom_command(TARGET ${target} POST_BUILD
 COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${target}>/Content/waterfall"
 COMMAND ${CMAKE_COMMAND} -E copy_if_different
 "${CMAKE_BINARY_DIR}/waterfall-studio-shaders/PlaceableWaterfallPS.cso"
 "${CMAKE_BINARY_DIR}/waterfall-studio-shaders/PlaceableWaterfallVS.cso"
 "${CMAKE_SOURCE_DIR}/EngineBridge/assets/waterfall-v2/AeratedWater.png"
 "${CMAKE_SOURCE_DIR}/EngineBridge/assets/waterfall-v2/SteamMist.png"
 "${CMAKE_SOURCE_DIR}/EngineBridge/assets/waterfall-v2/WaterWisp.png"
 "$<TARGET_FILE_DIR:${target}>/Content/waterfall" VERBATIM)
endfunction()
