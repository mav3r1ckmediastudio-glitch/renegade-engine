# LP06 Gate 3 production Runtime seam. Kept separate from the long-lived
# Runtime target declaration so the lifecycle boundary is explicit.
target_sources(RenegadeRuntimeBootstrap
    PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/src/RuntimePackageBootstrap.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/RuntimePackageBootstrap.h"
)
