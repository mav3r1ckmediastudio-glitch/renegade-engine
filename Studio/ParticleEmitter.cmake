if(NOT TARGET RenegadeEngineBridge)
    message(FATAL_ERROR "Particle emitter authoring requires RenegadeEngineBridge")
endif()
if(NOT TARGET RenegadeStudio)
    message(FATAL_ERROR "Particle emitter authoring requires RenegadeStudio")
endif()

target_sources(RenegadeEngineBridge PRIVATE
    "${CMAKE_SOURCE_DIR}/EngineBridge/include/renegade/bridge/ParticleEmitterService.h"
    "${CMAKE_SOURCE_DIR}/EngineBridge/src/ParticleEmitterService.cpp"
)

target_sources(RenegadeStudio PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/src/RenegadeParticleEmitterWorkspace.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/src/RenegadeParticleEmitterWorkspace.h"
)

# Build a CPU-only bridge regression alongside Studio so the normal Studio CI
# cannot register an emitter test binary that was never produced. The tests do
# not run the particle GPU simulation; they prove native ECS creation,
# serialization/identity, state round-trip and exact Wicked hierarchy offsets.
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
