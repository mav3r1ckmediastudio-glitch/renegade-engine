# LP06 Gate 3 production Runtime seam. Kept separate from the long-lived
# Runtime target declaration so the lifecycle boundary is explicit.
target_sources(RenegadeRuntimeBootstrap
    PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/src/RuntimePackageBootstrap.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/RuntimePackageBootstrap.h"
)

# Renegade already consumes Wicked's header-only json.hpp from the Editor
# include directory inside EngineBridge. Gate 3 needs the same parser in the
# Runtime bootstrap translation unit, but does not link or enable Wicked Editor.
target_include_directories(RenegadeRuntimeBootstrap
    PRIVATE
        "${PROJECT_SOURCE_DIR}/WickedEngine/Editor"
)
