if(NOT DEFINED RENEGADE_SOURCE_DIR)
    message(FATAL_ERROR "RENEGADE_SOURCE_DIR is required")
endif()

set(service_header "${RENEGADE_SOURCE_DIR}/EngineBridge/include/renegade/bridge/ParticleEmitterService.h")
set(service_source "${RENEGADE_SOURCE_DIR}/EngineBridge/src/ParticleEmitterService.cpp")
set(workspace_header "${RENEGADE_SOURCE_DIR}/Studio/src/RenegadeParticleEmitterWorkspace.h")
set(workspace_source "${RENEGADE_SOURCE_DIR}/Studio/src/RenegadeParticleEmitterWorkspace.cpp")
set(chrome_source "${RENEGADE_SOURCE_DIR}/Studio/src/RenegadePhysicsLabStudioChrome.cpp")
set(behavior_test "${RENEGADE_SOURCE_DIR}/Tests/ParticleEmitterTests.cpp")

foreach(required IN ITEMS
    "${service_header}" "${service_source}" "${workspace_header}"
    "${workspace_source}" "${chrome_source}" "${behavior_test}")
    if(NOT EXISTS "${required}")
        message(FATAL_ERROR "Particle emitter source contract missing ${required}")
    endif()
endforeach()

file(READ "${service_header}" service_header_text)
foreach(token IN ITEMS "ParticleEmitterState" "framesX" "framesY"
    "frameCount" "frameStart" "frameRate" "SetParticleEmitterParentCommand")
    string(FIND "${service_header_text}" "${token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Particle emitter service header missing ${token}")
    endif()
endforeach()

file(READ "${service_source}" service_source_text)
foreach(token IN ITEMS "Entity_CreateEmitter" "AssignNewPersistentEntityId"
    "ReusableAssetInstanceIdMetadataKey" "Component_Attach" "Component_Detach"
    "SetFrameBlendingEnabled" "SetDepthCollisionEnabled" "SetSPHEnabled"
    "SetOpacityCurveControl")
    string(FIND "${service_source_text}" "${token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Particle emitter native Wicked bridge missing ${token}")
    endif()
endforeach()

file(READ "${workspace_source}" workspace_source_text)
foreach(token IN ITEMS "CreateEmitterInFrontOfCamera" "TEXTURE / SPRITESHEET..."
    "CreatorTextureWorkflowService" "SetMaterialTextureAssetCommand"
    "SPRITE SHEET // ANIMATED PARTICLES" "FRAME RATE FPS"
    "NATIVE WICKED HIERARCHY")
    string(FIND "${workspace_source_text}" "${token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Particle emitter Studio workflow missing ${token}")
    endif()
endforeach()

file(READ "${chrome_source}" chrome_source_text)
foreach(token IN ITEMS "PARTICLE EMITTER" "CreateEmitterInFrontOfCamera"
    "SetParticleWorkspaceActive")
    string(FIND "${chrome_source_text}" "${token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Particle emitter ADD/Inspector integration missing ${token}")
    endif()
endforeach()

file(READ "${behavior_test}" behavior_test_text)
foreach(token IN ITEMS "PersistentEntityId" "SetParticleEmitterCommand"
    "SetParticleEmitterParentCommand" "Crate 002" "730")
    string(FIND "${behavior_test_text}" "${token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Particle emitter behavior regression missing ${token}")
    endif()
endforeach()
