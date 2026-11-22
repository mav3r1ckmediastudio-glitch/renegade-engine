if(NOT DEFINED RENEGADE_SOURCE_DIR)
    message(FATAL_ERROR "RENEGADE_SOURCE_DIR is required")
endif()

file(READ "${RENEGADE_SOURCE_DIR}/EngineBridge/src/VegetationService.cpp" WD01_SERVICE)
file(READ "${RENEGADE_SOURCE_DIR}/Studio/src/WD01Vegetation.cpp" WD01_STUDIO)
file(READ "${RENEGADE_SOURCE_DIR}/Studio/src/StudioApplication.cpp" WD01_APPLICATION)
file(READ "${RENEGADE_SOURCE_DIR}/Studio/CMakeLists.txt" WD01_STUDIO_CMAKE)
file(READ "${RENEGADE_SOURCE_DIR}/Runtime/CMakeLists.txt" WD01_RUNTIME_CMAKE)

function(wd01_require CONTENT NEEDLE MESSAGE_TEXT)
    string(FIND "${CONTENT}" "${NEEDLE}" POSITION)
    if(POSITION EQUAL -1)
        message(FATAL_ERROR "WD01 contract: ${MESSAGE_TEXT}")
    endif()
endfunction()

wd01_require("${WD01_SERVICE}" "preset.hairs[0]" "must source the pinned Wicked HairParticleSystem")
wd01_require("${WD01_SERVICE}" "chunk.grass.vertex_lengths[index] = target" "must persist native per-chunk masks")
wd01_require("${WD01_SERVICE}" "wi::HairParticleSystem::REBUILD_BUFFERS" "must use Wicked's native buffer rebuild path")
wd01_require("${WD01_SERVICE}" "mesh->aabb.transform(world)" "must broad-phase against transformed chunk mesh bounds")
wd01_require("${WD01_SERVICE}" "FinalizeWickedVegetationStroke" "must defer distribution finalization to completed strokes")
wd01_require("${WD01_SERVICE}" "ManualChunkKey" "must distinguish newly generated chunks from painted chunks")
wd01_require("${WD01_STUDIO}" "FILTER_TERRAIN" "viewport brush must pick native terrain")
wd01_require("${WD01_STUDIO}" "PaintWickedVegetation" "Studio must delegate painting through EngineBridge")
wd01_require("${WD01_STUDIO}" "WickedVegetationStrokeCommand" "completed strokes must be Undo/Redo commands")
wd01_require("${WD01_STUDIO}" "VEGETATION // WICKED GRASS" "grass UI must live in Renegade's Terrain inspector")
wd01_require("${WD01_STUDIO}" "A stroke can begin in the viewport and be released over Renegade's" "stroke release must be finalized outside the viewport")
wd01_require("${WD01_STUDIO}" "return leftPressed || leftDown || state.strokeActive;" "an armed brush must consume only left-button painting so viewport navigation remains available")
wd01_require("${WD01_APPLICATION}" "Cancel the persistent" "Renegade chrome must cancel vegetation input ownership")
wd01_require("${WD01_APPLICATION}" "const bool vegetationConsumed =\n            HandleWd01Vegetation(pointer);\n        bool riverConsumed=false;\n        {ScopedCPUProfiling(\"Studio river drawing\");\n        riverConsumed=HandleTerrainRiver(pointer);}\n\n        if (sunPreviewPlaying_)" "stroke finalization must run before UI callbacks can return from the frame")
# Resolve authoring-tool ownership before navigation or GUI focus can return.
foreach(token IN ITEMS "if (vegetationConsumed)" "if (riverConsumed)"
    "HandleViewportNavigation(dt, pointer)" "if (GetGUI().HasFocus() && !gizmoDragActive_)")
    string(FIND "${WD01_APPLICATION}" "${token}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "WD01 input owner missing: ${token}")
    endif()
    if(DEFINED previous_owner AND position LESS previous_owner)
        message(FATAL_ERROR "WD01 vegetation/river ownership must precede navigation and native GUI focus")
    endif()
    set(previous_owner ${position})
endforeach()
wd01_require("${WD01_APPLICATION}" "else\n        {\n            DisableWd01VegetationBrush();" "leaving Terrain must cancel the vegetation tool")
wd01_require("${WD01_STUDIO_CMAKE}" "WickedEngine/Content/terrain/grass.wiscene" "Studio must package Wicked grass.wiscene")
wd01_require("${WD01_STUDIO_CMAKE}" "WickedEngine/Content/terrain/grassparticle.png" "Studio must package Wicked grass texture")
wd01_require("${WD01_RUNTIME_CMAKE}" "WickedEngine/Content/terrain/grass.wiscene" "Runtime must package Wicked grass.wiscene")
wd01_require("${WD01_RUNTIME_CMAKE}" "WickedEngine/Content/terrain/grassparticle.png" "Runtime must package Wicked grass texture")

string(FIND "${WD01_STUDIO}" "HairParticleWindow" STOCK_WINDOW)
if(NOT STOCK_WINDOW EQUAL -1)
    message(FATAL_ERROR "WD01 contract: stock Wicked Editor UI must not be embedded")
endif()

# Every shipped palette choice needs an RGBA cutout in its own group.
set(plant_root "${RENEGADE_SOURCE_DIR}/Content/terrain/foliage")
set(stock_plants
    "01_short_grass_albedo.png"
    "02_long_grass_albedo.png"
    "03_dead_grass_albedo.png"
    "04_burnt_grass_albedo.png"
    "05_wheat_albedo.png"
    "06_daisies_albedo.png"
    "07_poppies_albedo.png"
    "08_nettles_albedo.png"
    "09_clover_albedo.png"
    "10_thistles_albedo.png"
    "Alien/01_lumen_grass_albedo.png"
    "Alien/02_crimson_razorgrass_albedo.png"
    "Alien/03_spore_grass_albedo.png"
    "Alien/04_crystal_grass_albedo.png"
    "Alien/05_velvet_frond_albedo.png"
    "Alien/06_glowcap_grass_albedo.png"
    "Alien/07_void_leaf_grass_albedo.png"
    "Alien/08_curlgrass_albedo.png"
    "Alien/09_coral_grass_albedo.png"
    "Alien/10_rift_blossom_albedo.png"
    "Freshwater/06_vallisneria_albedo.png"
    "Freshwater/07_amazon_sword_albedo.png"
    "Freshwater/08_anubias_albedo.png"
    "Freshwater/09_java_fern_albedo.png"
    "Freshwater/10_red_ludwigia_albedo.png"
    "Marine/01_coastal_seagrass_albedo.png"
    "Marine/02_sea_lettuce_albedo.png"
    "Marine/03_red_algae_albedo.png"
    "Marine/04_feather_algae_albedo.png"
    "Marine/05_golden_kelp_albedo.png"
)
foreach(plant IN LISTS stock_plants)
    file(READ "${plant_root}/${plant}" png_header OFFSET 0 LIMIT 26 HEX)
    if(NOT png_header MATCHES "^89504e470d0a1a0a0000000d4948445200000100000001000806$")
        message(FATAL_ERROR "Plant palette requires a 256 x 256 RGBA cutout at ${plant}")
    endif()
endforeach()
