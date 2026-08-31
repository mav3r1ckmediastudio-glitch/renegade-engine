file(READ "${RENEGADE_SOURCE_DIR}/EngineBridge/include/renegade/bridge/VegetationService.h" WD01_HEADER)
file(READ "${RENEGADE_SOURCE_DIR}/EngineBridge/src/VegetationService.cpp" WD01_SERVICE)
file(READ "${RENEGADE_SOURCE_DIR}/Studio/src/WD01Vegetation.cpp" WD01_STUDIO)
file(READ "${RENEGADE_SOURCE_DIR}/Studio/src/StudioApplication.cpp" WD01_APP)
file(READ "${RENEGADE_SOURCE_DIR}/Studio/CMakeLists.txt" WD01_STUDIO_CMAKE)

function(wd01_require HAYSTACK NEEDLE MESSAGE_TEXT)
    string(FIND "${${HAYSTACK}}" "${NEEDLE}" FOUND)
    if(FOUND EQUAL -1)
        message(FATAL_ERROR "WD01 contract: ${MESSAGE_TEXT}")
    endif()
endfunction()

wd01_require(WD01_HEADER "VegetationBrushMode" "missing Paint/Delete brush contract")
wd01_require(WD01_HEADER "VegetationStrokeCommand" "missing vegetation Undo/Redo command")
wd01_require(WD01_SERVICE "wi::scene::LoadModel(grassScene, grassScenePath)" "must consume native Wicked grass.wiscene")
wd01_require(WD01_SERVICE "vertex_lengths" "must author native Wicked hair vertex mask")
wd01_require(WD01_SERVICE "CreateFromMesh" "must rebuild native Wicked hair distribution")
wd01_require(WD01_STUDIO "VEGETATION // GRASS" "missing Terrain vegetation section")
wd01_require(WD01_STUDIO "PAINT" "missing creator Paint tool")
wd01_require(WD01_STUDIO "DELETE" "missing creator Delete tool")
wd01_require(WD01_APP "HandleWd01Vegetation(pointer)" "viewport does not route vegetation brush")
wd01_require(WD01_STUDIO_CMAKE "WickedEngine/Content/terrain/grass.wiscene" "Studio does not package Wicked grass scene")
wd01_require(WD01_STUDIO_CMAKE "WickedEngine/Content/terrain/grassparticle.png" "Studio does not package Wicked grass texture")

# Terrain's pre-WD01 action rows are anchored at Y 726 and end at Y 834
# (save row at 806 with 28 px controls). Vegetation must append below that
# boundary so Wicked's normal Window scroll-range calculation can expose it
# without overlapping Undo/Redo/Save controls.
wd01_require(WD01_APP "? 726.0f" "Terrain action-row anchor changed; re-audit vegetation layout boundary")
wd01_require(WD01_STUDIO "TerrainInspectorActionsBottom = 834.0f" "missing audited Terrain action-row bottom boundary")
wd01_require(WD01_STUDIO "VegetationInspectorTop =" "missing vegetation scroll-extension anchor")
wd01_require(WD01_STUDIO "TerrainInspectorActionsBottom + VegetationInspectorGap" "vegetation must begin after existing Terrain action rows")
wd01_require(WD01_STUDIO "place(controls.sectionLabel, VegetationInspectorTop, 20.0f)" "vegetation section is not anchored below Terrain actions")
wd01_require(WD01_STUDIO "place(controls.status, VegetationInspectorTop + 148.0f, 34.0f)" "vegetation content does not extend the Terrain scroll range")

foreach(FORBIDDEN IN ITEMS "HairParticleWindow" "PaintToolWindow")
    string(FIND "${WD01_STUDIO}" "${FORBIDDEN}" FOUND)
    if(NOT FOUND EQUAL -1)
        message(FATAL_ERROR "WD01 contract: stock Wicked Editor UI leaked into Renegade Studio: ${FORBIDDEN}")
    endif()
endforeach()

message(STATUS "WD01 vegetation source contract passed")
