if(NOT TARGET RenegadeStudio)
    message(FATAL_ERROR "Phase 6 Gate 3 requires the RenegadeStudio target")
endif()

target_sources(RenegadeStudio PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/src/RenegadeAudioWorkspace.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/src/RenegadeAudioWorkspace.h"
)
