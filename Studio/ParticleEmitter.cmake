if(NOT TARGET RenegadeEngineBridge)
    message(FATAL_ERROR "Particle emitter authoring requires RenegadeEngineBridge")
endif()
if(NOT TARGET RenegadeStudio)
    message(FATAL_ERROR "Particle emitter authoring requires RenegadeStudio")
endif()

target_sources(RenegadeEngineBridge PRIVATE
    "${CMAKE_SOURCE_DIR}/EngineBridge/include/renegade/bridge/ParticleEmitterService.h"
    "${CMAKE_SOURCE_DIR}/EngineBridge/src/ParticleEmitterService.cpp"
    "${CMAKE_SOURCE_DIR}/EngineBridge/include/renegade/bridge/ParticleBlendModeService.h"
    "${CMAKE_SOURCE_DIR}/EngineBridge/src/ParticleBlendModeService.cpp"
)

target_sources(RenegadeStudio PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/src/RenegadeParticleEmitterWorkspaceV3.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/src/RenegadeParticleEmitterWorkspace.h"
)

# Build a CPU-only bridge regression alongside Studio so the normal Studio CI
# cannot register an emitter test binary that was never produced. The tests do
# not run the particle GPU simulation; they prove native ECS creation,
# serialization/identity, state round-trip, blend mode and exact Wicked hierarchy offsets.
add_executable(RenegadeParticleEmitterTests
    "${CMAKE_SOURCE_DIR}/Tests/ParticleEmitterTests.cpp"
)
target_link_libraries(RenegadeParticleEmitterTests PRIVATE Renegade::EngineBridge)
target_compile_options(RenegadeParticleEmitterTests PRIVATE
    "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>"
)
set_target_properties(RenegadeParticleEmitterTests PROPERTIES
    FOLDER "Renegade/Tests"
)
add_dependencies(RenegadeStudio RenegadeParticleEmitterTests)
add_test(NAME RenegadeParticleEmitterTests COMMAND RenegadeParticleEmitterTests)

add_test(
    NAME RenegadeParticleEmitterSourceContract
    COMMAND ${CMAKE_COMMAND}
        -DRENEGADE_SOURCE_DIR=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_SOURCE_DIR}/Tests/ParticleEmitterSourceContract.cmake
)

# Owner-acceptance UI contract: the compiled V3 shim must keep numeric value
# boxes exclusive from slider hit testing and keep open combo popups opaque and
# interaction-exclusive above the controls they overlap.
add_test(
    NAME RenegadeParticleEmitterOwnerFeedbackSourceContract
    COMMAND ${CMAKE_COMMAND}
        -DRENEGADE_SOURCE_DIR=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_SOURCE_DIR}/Tests/ParticleEmitterOwnerFeedbackSourceContract.cmake
)
