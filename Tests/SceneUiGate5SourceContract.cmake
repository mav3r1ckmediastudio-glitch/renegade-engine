if(NOT DEFINED RENEGADE_SOURCE_DIR)
    message(FATAL_ERROR "RENEGADE_SOURCE_DIR is required")
endif()

set(TERRAIN_HEADER "${RENEGADE_SOURCE_DIR}/EngineBridge/include/renegade/bridge/TerrainService.h")
set(TERRAIN_SOURCE "${RENEGADE_SOURCE_DIR}/EngineBridge/src/TerrainService.cpp")
set(PRECIPITATION_SOURCE "${RENEGADE_SOURCE_DIR}/EngineBridge/src/PrecipitationService.cpp")
set(STUDIO_SOURCE "${RENEGADE_SOURCE_DIR}/Studio/src/StudioApplication.cpp")
set(STUDIO_CMAKE "${RENEGADE_SOURCE_DIR}/Studio/CMakeLists.txt")
set(RUNTIME_CMAKE "${RENEGADE_SOURCE_DIR}/Runtime/CMakeLists.txt")

foreach(path IN ITEMS
    "${TERRAIN_HEADER}"
    "${TERRAIN_SOURCE}"
    "${PRECIPITATION_SOURCE}"
    "${STUDIO_SOURCE}"
    "${STUDIO_CMAKE}"
    "${RUNTIME_CMAKE}")
    if(NOT EXISTS "${path}")
        message(FATAL_ERROR "Scene UI Gate 5 source contract input is missing: ${path}")
    endif()
endforeach()

file(READ "${TERRAIN_HEADER}" terrain_header)
file(READ "${TERRAIN_SOURCE}" terrain_source)
file(READ "${PRECIPITATION_SOURCE}" precipitation_source)
file(READ "${STUDIO_SOURCE}" studio_source)
file(READ "${STUDIO_CMAKE}" studio_cmake)
file(READ "${RUNTIME_CMAKE}" runtime_cmake)

function(require_text haystack_var needle description)
    string(FIND "${${haystack_var}}" "${needle}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Scene UI Gate 5 contract missing ${description}: ${needle}")
    endif()
endfunction()

function(reject_text haystack_var needle description)
    string(FIND "${${haystack_var}}" "${needle}" found)
    if(NOT found EQUAL -1)
        message(FATAL_ERROR "Scene UI Gate 5 contract retained ${description}: ${needle}")
    endif()
endfunction()

require_text(terrain_header "DefaultTerrainChunkRadius = 9" "19x19 standard radius")
require_text(terrain_header "float chunkScale = 1.0f;" "one-metre vertex spacing")
require_text(terrain_header "class ExpandTerrainCommand final" "undoable finite expansion")
require_text(terrain_source "terrain->Generation_Cancel();" "safe generation boundary")
require_text(terrain_source "terrain->generation = afterRadius_;" "missing-ring generation request")
reject_text(studio_source "Terrain Visible Radius" "raw destructive radius control")
require_text(studio_source "EXPAND TERRAIN // +1 RING" "creator expansion control")
require_text(studio_source "CURRENT TERRAIN //" "honest terrain dimensions")
require_text(studio_source "WeatherField::Stars" "native Stars authoring")

foreach(map IN ITEMS basecolor normal surface)
    require_text(terrain_source "default_grass_${map}.dds" "DDS ${map} runtime binding")
    require_text(studio_cmake "default_grass" "Studio terrain resource packaging")
    require_text(runtime_cmake "default_grass" "Runtime terrain resource packaging")
endforeach()
reject_text(terrain_source "default_grass_basecolor.tga\";" "TGA default runtime binding")

require_text(precipitation_source "snowflake.dds" "dedicated snow visual")
require_text(studio_cmake "assets/weather/snowflake.dds" "Studio snow packaging")
require_text(runtime_cmake "assets/weather/snowflake.dds" "Runtime snow packaging")

message(STATUS "Scene UI Gate 5 Environment and finite terrain source contract passed")
