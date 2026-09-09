# Renegade-owned, editor-only viewport marker system.
#
# Marker artwork is intentionally NOT part of the repository/build graph. The
# stock PNGs are added manually to the completed Studio build at:
#   Content/Editor/MarkerIcons
# This keeps binary artwork out of Git/CI while preserving the runtime lookup
# order implemented by MarkerIconOverlay:
#   Project/Content/Editor/MarkerIcons -> User/MarkerIcons -> stock Content/Editor/MarkerIcons.

target_sources(RenegadeStudio PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/src/MarkerIconOverlay.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/src/MarkerIconOverlay.h"
)

add_test(
    NAME RenegadeMarkerIconsSourceContract
    COMMAND ${CMAKE_COMMAND}
        -DRENEGADE_SOURCE_DIR=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_SOURCE_DIR}/Tests/MarkerIconsSourceContract.cmake
)

# Kept on the isolated VFX follow-up branch while #146 owner acceptance CI is
# running. The follow-up remains a separate commit series / no-PR branch.
include("${CMAKE_CURRENT_LIST_DIR}/ParticleVfxFollowup.cmake")
