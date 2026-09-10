if(NOT TARGET RenegadeStudio)
    message(FATAL_ERROR "Phase 6 navigation requires the RenegadeStudio target")
endif()

target_sources(RenegadeStudio PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/src/RenegadeNavigationWorkspace.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/src/RenegadeNavigationWorkspace.h"
)
