# Staging integration for the creator-facing VFX follow-up. This file is
# deliberately included by the existing Windows Studio marker integration so
# work can compile on its isolated branch without changing the running #146 PR.

target_sources(RenegadeEngineBridge PRIVATE
    "${CMAKE_SOURCE_DIR}/EngineBridge/include/renegade/bridge/ParticleBlendModeService.h"
    "${CMAKE_SOURCE_DIR}/EngineBridge/src/ParticleBlendModeService.cpp"
    "${CMAKE_SOURCE_DIR}/EngineBridge/include/renegade/bridge/ParticleEffectService.h"
    "${CMAKE_SOURCE_DIR}/EngineBridge/src/ParticleEffectService.cpp"
    "${CMAKE_SOURCE_DIR}/EngineBridge/include/renegade/bridge/ParticleEffectLibraryService.h"
    "${CMAKE_SOURCE_DIR}/EngineBridge/src/ParticleEffectLibraryService.cpp"
)

add_executable(RenegadeParticleVfxFollowupTests
    "${CMAKE_SOURCE_DIR}/Tests/ParticleVfxFollowupTests.cpp"
)
target_link_libraries(RenegadeParticleVfxFollowupTests PRIVATE
    Renegade::EngineBridge
)
set_target_properties(RenegadeParticleVfxFollowupTests PROPERTIES
    FOLDER "Renegade/Tests"
)
add_test(
    NAME RenegadeParticleVfxFollowupTests
    COMMAND RenegadeParticleVfxFollowupTests
)

add_test(
    NAME RenegadeParticleVfxFollowupSourceContract
    COMMAND ${CMAKE_COMMAND}
        -DRENEGADE_SOURCE_DIR=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_SOURCE_DIR}/Tests/ParticleVfxFollowupSourceContract.cmake
)
