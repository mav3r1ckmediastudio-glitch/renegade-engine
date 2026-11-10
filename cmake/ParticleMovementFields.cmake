add_executable(RenegadeParticleFieldShaderCompiler "${CMAKE_SOURCE_DIR}/Tools/ParticleFieldShaderCompiler.cpp")
target_link_libraries(RenegadeParticleFieldShaderCompiler PRIVATE Renegade::EngineBridge)
set(RENEGADE_FIELD_STAGE "${CMAKE_BINARY_DIR}/particle-field-shaders")
set(RENEGADE_FIELD_FILES)
foreach(index RANGE 0 6)
 list(APPEND RENEGADE_FIELD_FILES "${RENEGADE_FIELD_STAGE}/MovementField${index}.cso")
endforeach()
add_custom_command(OUTPUT ${RENEGADE_FIELD_FILES}
 COMMAND $<TARGET_FILE:RenegadeParticleFieldShaderCompiler>
  "${CMAKE_SOURCE_DIR}/WickedEngine/WickedEngine/shaders" "${CMAKE_SOURCE_DIR}/EngineBridge/shaders" "${RENEGADE_FIELD_STAGE}"
 DEPENDS RenegadeParticleFieldShaderCompiler
 "${CMAKE_SOURCE_DIR}/EngineBridge/shaders/renegade_particle_fieldsHF.hlsli"
 "${CMAKE_SOURCE_DIR}/WickedEngine/WickedEngine/shaders/emittedparticle_simulateCS.hlsl"
 "${CMAKE_SOURCE_DIR}/WickedEngine/WickedEngine/shaders/emittedparticle_emitCS.hlsl"
 "${CMAKE_SOURCE_DIR}/WickedEngine/WickedEngine/shaders/ShaderInterop_EmittedParticle.h"
 VERBATIM)
add_custom_target(RenegadeParticleMovementShaders DEPENDS ${RENEGADE_FIELD_FILES})
function(renegade_deploy_particle_fields target)
 add_dependencies(${target} RenegadeParticleMovementShaders)
 add_custom_command(TARGET ${target} POST_BUILD
 COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${target}>/Content/particles"
 COMMAND ${CMAKE_COMMAND} -E copy_if_different ${RENEGADE_FIELD_FILES} "$<TARGET_FILE_DIR:${target}>/Content/particles" VERBATIM)
endfunction()
