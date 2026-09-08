# Renegade-owned, editor-only viewport marker system.
# Stock artwork is copied beside Studio and may be overridden without recompiling:
#   Project/Content/Editor/MarkerIcons -> User/MarkerIcons -> stock Content/Editor/MarkerIcons.

target_sources(RenegadeStudio PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/src/MarkerIconOverlay.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/src/MarkerIconOverlay.h"
)

set(RENEGADE_MARKER_ICON_SOURCE
    "${CMAKE_CURRENT_LIST_DIR}/assets/markericons")
set(RENEGADE_MARKER_ICON_OUTPUT
    "${CMAKE_BINARY_DIR}/Studio/$<CONFIG>/Content/Editor/MarkerIcons")

add_custom_target(RenegadeMarkerIconAssets
    COMMAND ${CMAKE_COMMAND} -E remove_directory
        "${RENEGADE_MARKER_ICON_OUTPUT}"
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "${CMAKE_BINARY_DIR}/Studio/$<CONFIG>/Content/Editor"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${RENEGADE_MARKER_ICON_SOURCE}"
        "${RENEGADE_MARKER_ICON_OUTPUT}"
    VERBATIM
)
add_dependencies(RenegadeStudio RenegadeMarkerIconAssets)
set_target_properties(RenegadeMarkerIconAssets PROPERTIES
    FOLDER "Renegade/Packaging"
)

add_test(
    NAME RenegadeMarkerIconsSourceContract
    COMMAND ${CMAKE_COMMAND}
        -DRENEGADE_SOURCE_DIR=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_SOURCE_DIR}/Tests/MarkerIconsSourceContract.cmake
)
