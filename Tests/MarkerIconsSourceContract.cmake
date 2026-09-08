set(marker_root "${RENEGADE_SOURCE_DIR}/Studio/assets/markericons")
set(marker_source "${RENEGADE_SOURCE_DIR}/Studio/src/MarkerIconOverlay.cpp")
set(marker_cmake "${RENEGADE_SOURCE_DIR}/Studio/MarkerIcons.cmake")
set(diagnostics "${RENEGADE_SOURCE_DIR}/Studio/src/StudioLiveDiagnostics.cpp")
set(root_cmake "${RENEGADE_SOURCE_DIR}/CMakeLists.txt")

foreach(file IN ITEMS
    player_start.png
    light_point.png
    light_spot.png
    light_directional.png
    light_rectangle.png
    camera.png
    decal_projector.png
    audio_source.png
    particle_emitter.png
    trigger_zone.png
    audio_zone.png
    npc_spawn.png
    waypoint.png
    checkpoint.png
    pickup_spawn.png
    script_entity.png
    physics_volume.png)
    if(NOT EXISTS "${marker_root}/${file}")
        message(FATAL_ERROR "MarkerIcons stock asset missing: ${file}")
    endif()
endforeach()

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

require_text(marker_text "projectRoot" "project override seam")
require_text(marker_text "fs::path(\"User\") / \"MarkerIcons\"" "user override seam")
require_text(marker_text "fs::path(\"Content\") / \"Editor\" / \"MarkerIcons\"" "stock fallback seam")
require_text(marker_text "ParticleEmitter" "particle marker registration")
require_text(marker_text "ScriptEntity" "script marker registration")
require_text(marker_cmake_text "copy_directory" "stock marker packaging")
require_text(marker_cmake_text "Content/Editor/MarkerIcons" "packaged marker destination")
require_text(diagnostics_text "EnsureMarkerIconOverlay(*this);" "frame lifecycle registration")
require_text(root_cmake_text "include(Studio/MarkerIcons.cmake)" "root build registration")
