if(NOT TARGET RenegadeStudio)
    message(FATAL_ERROR "Phase 6 Gate 3 requires the RenegadeStudio target")
endif()

target_sources(RenegadeStudio PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/src/RenegadeAudioWorkspace.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/src/RenegadeAudioWorkspace.h"
)

# Particle authoring is a bounded post-S7 morale feature: native Wicked GPU
# emitters, transform/hierarchy and material texture bindings, wrapped only in
# Renegade-owned creator UI. Keep its build registration isolated.
include("${CMAKE_CURRENT_LIST_DIR}/ParticleEmitter.cmake")

# Native navigation is staged beside the existing specialist Scene workspaces.
# It remains an EngineBridge adapter over Wicked VoxelGrid/PathQuery rather than
# a second navigation world.
include("${CMAKE_CURRENT_LIST_DIR}/Phase6Navigation.cmake")
