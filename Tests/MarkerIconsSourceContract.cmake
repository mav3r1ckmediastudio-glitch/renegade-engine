set(marker_source "${RENEGADE_SOURCE_DIR}/Studio/src/MarkerIconOverlay.cpp")
set(marker_cmake "${RENEGADE_SOURCE_DIR}/Studio/MarkerIcons.cmake")
set(diagnostics "${RENEGADE_SOURCE_DIR}/Studio/src/StudioLiveDiagnostics.cpp")
set(root_cmake "${RENEGADE_SOURCE_DIR}/CMakeLists.txt")
set(marker_asset_root "${RENEGADE_SOURCE_DIR}/Studio/Content/editor/markericons")

foreach(required IN ITEMS "${marker_source}" "${marker_cmake}" "${diagnostics}" "${root_cmake}")
    if(NOT EXISTS "${required}")
        message(FATAL_ERROR "MarkerIcons integration file missing: ${required}")
    endif()
endforeach()

file(READ "${marker_source}" marker_text)
file(READ "${marker_cmake}" marker_cmake_text)
file(READ "${diagnostics}" diagnostics_text)
file(READ "${root_cmake}" root_cmake_text)

function(require_text haystack needle description)
    string(FIND "${${haystack}}" "${needle}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "MarkerIcons contract failed: ${description}")
    endif()
endfunction()

function(reject_text haystack needle description)
    string(FIND "${${haystack}}" "${needle}" position)
    if(NOT position EQUAL -1)
        message(FATAL_ERROR "MarkerIcons contract failed: ${description}")
    endif()
endfunction()

require_text(marker_text "projectRoot" "project override seam")
require_text(marker_text "fs::path(\"User\") / \"MarkerIcons\"" "user override seam")
require_text(marker_text "fs::path(\"Content\") / \"Editor\" / \"MarkerIcons\"" "stock fallback seam")
require_text(marker_text "ParticleEmitter" "particle marker registration")
require_text(marker_text "ScriptEntity" "script marker registration")
require_text(marker_text "constexpr float MarkerSize = 96.0f" "owner-approved 200 percent marker size")
require_text(marker_text "SetEnabled(true)" "interactive marker overlay")
require_text(marker_text "session->Selection().Select(candidate)" "viewport marker click selection")
require_text(marker_text "bestDistance2" "overlapping marker nearest-centre selection")
require_text(marker_cmake_text "MarkerIconOverlay.cpp" "marker overlay Studio registration")
require_text(marker_cmake_text "Content/editor/markericons" "tracked marker artwork source")
require_text(marker_cmake_text "copy_directory" "loose marker artwork build staging")
require_text(marker_cmake_text "add_dependencies(RenegadeStudio RenegadeMarkerIconAssets)" "Studio asset staging dependency")
require_text(diagnostics_text "EnsureMarkerIconOverlay(*this);" "frame lifecycle registration")
require_text(root_cmake_text "include(Studio/MarkerIcons.cmake)" "root build registration")

set(expected_marker_icons
    audio_source.png
    audio_zone.png
    camera.png
    checkpoint.png
    decal_projector.png
    light_directional.png
    light_point.png
    light_rectangle.png
    light_spot.png
    npc_spawn.png
    particle_emitter.png
    physics_volume.png
    pickup_spawn.png
    player_start.png
    script_entity.png
    trigger_zone.png
    waypoint.png
)

file(GLOB actual_marker_icons RELATIVE "${marker_asset_root}" "${marker_asset_root}/*.png")
list(SORT actual_marker_icons)
list(SORT expected_marker_icons)
if(NOT actual_marker_icons STREQUAL expected_marker_icons)
    message(FATAL_ERROR
        "MarkerIcons contract failed: expected exact stock PNG set; got ${actual_marker_icons}")
endif()

foreach(marker_icon IN LISTS expected_marker_icons)
    set(marker_path "${marker_asset_root}/${marker_icon}")
    file(READ "${marker_path}" png_signature OFFSET 0 LIMIT 8 HEX)
    if(NOT png_signature STREQUAL "89504e470d0a1a0a")
        message(FATAL_ERROR "MarkerIcons contract failed: invalid PNG signature for ${marker_icon}")
    endif()
endforeach()
