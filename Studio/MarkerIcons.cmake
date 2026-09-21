# Renegade-owned, editor-only viewport marker system.
#
# Stock artwork is tracked under Studio/Content and copied as loose files into
# the compiled Studio Content tree. Keeping the archive out of the runtime
# package lets MarkerIconOverlay load each PNG directly while preserving its
# override order:
#   Project/Content/Editor/MarkerIcons -> User/MarkerIcons -> stock Content/Editor/MarkerIcons.

target_sources(RenegadeStudio PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/src/MarkerIconOverlay.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/src/MarkerIconOverlay.h"
)

set(RENEGADE_MARKER_ICON_OUTPUT
    "${CMAKE_BINARY_DIR}/Studio/$<CONFIG>/Content/editor/markericons")
add_custom_target(RenegadeMarkerIconAssets
    COMMAND ${CMAKE_COMMAND} -E remove_directory
        "${RENEGADE_MARKER_ICON_OUTPUT}"
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "${CMAKE_BINARY_DIR}/Studio/$<CONFIG>/Content/editor"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_LIST_DIR}/Content/editor/markericons"
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
