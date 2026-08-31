file(READ "${RENEGADE_SOURCE_DIR}/EngineBridge/include/renegade/bridge/VegetationService.h" WD01_HEADER)
file(READ "${RENEGADE_SOURCE_DIR}/EngineBridge/src/VegetationService.cpp" WD01_SERVICE)
file(READ "${RENEGADE_SOURCE_DIR}/EngineBridge/src/VegetationNativePaintService.cpp" WD01_NATIVE_PAINT)
file(READ "${RENEGADE_SOURCE_DIR}/EngineBridge/include/renegade/bridge/VegetationAppearanceService.h" WD01_APPEARANCE)
file(READ "${RENEGADE_SOURCE_DIR}/EngineBridge/CMakeLists.txt" WD01_BRIDGE_CMAKE)
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
wd01_require(WD01_SERVICE "if (texture.name == texturePath && texture.resource.IsValid())" "grass material rebind is no longer idempotent")
wd01_require(WD01_STUDIO "VEGETATION // GRASS" "missing Terrain vegetation section")
wd01_require(WD01_STUDIO "PAINT" "missing creator Paint tool")
wd01_require(WD01_STUDIO "DELETE" "missing creator Delete tool")
wd01_require(WD01_APP "HandleWd01Vegetation(pointer)" "viewport does not route vegetation brush")
wd01_require(WD01_STUDIO_CMAKE "WickedEngine/Content/terrain/grass.wiscene" "Studio does not package Wicked grass scene")
wd01_require(WD01_STUDIO_CMAKE "WickedEngine/Content/terrain/grassparticle.png" "Studio does not package Wicked grass texture")

# Painting is routed through the native coverage implementation. The legacy
# support file keeps initialization/Undo/Redo only; it must not be the linked
# PaintVegetation symbol.
wd01_require(WD01_BRIDGE_CMAKE "src/VegetationNativePaintService.cpp" "native Wicked paint implementation is not compiled")
wd01_require(WD01_BRIDGE_CMAKE "PaintVegetation=PaintVegetationLegacy" "legacy custom painter is still the linked PaintVegetation symbol")

# Paint/Delete must follow Wicked HairParticle Add/Remove coverage semantics:
# real zero means no emitter support, Paint writes 1, Delete writes 0.
wd01_require(WD01_NATIVE_PAINT "mode == VegetationBrushMode::Paint ? 1.0f : 0.0f" "Paint/Delete no longer maps to native Add/Remove coverage values")
wd01_require(WD01_NATIVE_PAINT "chunk.grass.vertex_lengths[vertex] = target" "painting no longer edits the authored Wicked vertex mask")
wd01_require(WD01_NATIVE_PAINT "CountActiveGrassVertices" "active grass coverage is not counted")
wd01_require(WD01_NATIVE_PAINT "NativeTerrainStrandCount" "native terrain strand-density formula is missing")
wd01_require(WD01_NATIVE_PAINT "activeVertexCount) * 3.0f" "strand capacity is not derived from actual painted vertices")
wd01_require(WD01_NATIVE_PAINT "live->_flags |= wi::HairParticleSystem::REBUILD_BUFFERS" "live painting no longer uses Wicked HairParticle rebuild semantics")
wd01_require(WD01_NATIVE_PAINT "scene.Entity_Remove(chunk.grass_entity)" "fully empty chunks must release their live grass entity")
wd01_require(WD01_NATIVE_PAINT "substepCount" "drag stroke subdivision was removed")

# The discarded full-chunk 1/255 system is migration-only. It must never be
# used to activate an empty chunk again because that creates full strand cost
# for tiny painted patches and leaves stale chunks unpaintable.
wd01_require(WD01_NATIVE_PAINT "MigrateLegacyStableChunks" "legacy stable-distribution scenes are not migrated")
wd01_require(WD01_NATIVE_PAINT "LegacyInvisibleLength = 1.0f / 255.0f" "legacy R8 support migration threshold changed")
string(FIND "${WD01_NATIVE_PAINT}" "mesh.vertex_positions.size(), LegacyInvisibleLength" FULL_CHUNK_STABLE)
if(NOT FULL_CHUNK_STABLE EQUAL -1)
    message(FATAL_ERROR "WD01 contract: full-chunk 1/255 activation must not return")
endif()
string(FIND "${WD01_NATIVE_PAINT}" "if (current <= 0.0f)" ZERO_BLOCK)
if(NOT ZERO_BLOCK EQUAL -1)
    message(FATAL_ERROR "WD01 contract: zero-length vertices must remain paintable by Add mode")
endif()

# Targeting must follow Wicked PaintTool's object-space broad phase: brush
# sphere against the current transformed mesh AABB, then exact vertex distance.
# Do not use Terrain::ChunkData::sphere bookkeeping as a paint eligibility gate.
wd01_require(WD01_NATIVE_PAINT "scene.transforms.GetComponent(chunk.entity)" "native painter must transform terrain mesh vertices with the chunk entity world matrix")
wd01_require(WD01_NATIVE_PAINT "mesh->aabb.transform(world)" "grass paint broad phase must use the current transformed chunk mesh AABB")
wd01_require(WD01_NATIVE_PAINT "brushSphere.intersects(worldAabb)" "grass paint broad phase no longer mirrors Wicked sphere/AABB targeting")
wd01_require(WD01_NATIVE_PAINT "XMVector3TransformCoord(position, world)" "terrain vertex painting must use the chunk world transform")
string(FIND "${WD01_NATIVE_PAINT}" "chunk.sphere" CHUNK_SPHERE_GATE)
if(NOT CHUNK_SPHERE_GATE EQUAL -1)
    message(FATAL_ERROR "WD01 contract: stale ChunkData::sphere must not gate grass painting")
endif()
string(FIND "${WD01_NATIVE_PAINT}" "scene.transforms.GetComponent(\n                chunk.grass_entity)" GRASS_CHILD_WORLD)
if(NOT GRASS_CHILD_WORLD EQUAL -1)
    message(FATAL_ERROR "WD01 contract: first-contact painting must not use an unsynchronised grass-child world matrix")
endif()

# The failed experimental uploader must never return. Wicked owns HairParticle
# GPU rebuilding; Renegade must not create an ad-hoc UpdateBuffer paint path.
string(FIND "${WD01_NATIVE_PAINT}" "UpdateBuffer" NATIVE_UPLOAD)
if(NOT NATIVE_UPLOAD EQUAL -1)
    message(FATAL_ERROR "WD01 contract: custom GPU mask uploader leaked into native paint path")
endif()
string(FIND "${WD01_STUDIO}" "VegetationMaskUploadService" STUDIO_UPLOAD)
if(NOT STUDIO_UPLOAD EQUAL -1)
    message(FATAL_ERROR "WD01 contract: Studio still includes the failed custom GPU mask uploader")
endif()

# Appearance remains additive and must not own HairParticle topology/rebuilds.
wd01_require(WD01_APPEARANCE "VegetationAppearanceSettings" "missing isolated grass appearance settings contract")
wd01_require(WD01_APPEARANCE "hair.length = settings.length" "Length is not mapped to native Wicked hair")
wd01_require(WD01_APPEARANCE "hair.width = settings.width" "Width is not mapped to native Wicked hair")
wd01_require(WD01_APPEARANCE "hair.randomness = settings.randomness" "Randomness is not mapped to native Wicked hair")
wd01_require(WD01_APPEARANCE "hair.randomSeed = settings.randomSeed" "Random Seed is not mapped to native Wicked hair")
wd01_require(WD01_APPEARANCE "hair.uniformity = settings.uniformity" "Uniformity is not mapped to native Wicked hair")
wd01_require(WD01_STUDIO "APPEARANCE [-]" "missing collapsible Appearance section")
wd01_require(WD01_STUDIO "WD01 Grass Length" "missing grass Length control")
wd01_require(WD01_STUDIO "WD01 Grass Width" "missing grass Width control")
wd01_require(WD01_STUDIO "WD01 Grass Randomness" "missing grass Randomness control")
wd01_require(WD01_STUDIO "WD01 Grass Random Seed" "missing grass Random Seed control")
wd01_require(WD01_STUDIO "WD01 Grass Uniformity" "missing grass Uniformity control")
string(FIND "${WD01_APPEARANCE}" "REBUILD_BUFFERS" APPEARANCE_REBUILD)
if(NOT APPEARANCE_REBUILD EQUAL -1)
    message(FATAL_ERROR "WD01 contract: Appearance controls must not rebuild HairParticle emitter buffers")
endif()

# Vegetation appends after the existing Terrain inspector action rows.
wd01_require(WD01_APP "? 726.0f" "Terrain action-row anchor changed; re-audit vegetation layout boundary")
wd01_require(WD01_STUDIO "TerrainInspectorActionsBottom = 834.0f" "missing audited Terrain action-row bottom boundary")
wd01_require(WD01_STUDIO "TerrainInspectorActionsBottom + VegetationInspectorGap" "vegetation must begin after existing Terrain action rows")
wd01_require(WD01_STUDIO "place(controls.appearanceHeader, VegetationInspectorTop + 190.0f)" "Appearance section is not appended below paint controls")

foreach(FORBIDDEN IN ITEMS "HairParticleWindow" "PaintToolWindow")
    string(FIND "${WD01_STUDIO}" "${FORBIDDEN}" FOUND)
    if(NOT FOUND EQUAL -1)
        message(FATAL_ERROR "WD01 contract: stock Wicked Editor UI leaked into Renegade Studio: ${FORBIDDEN}")
    endif()
endforeach()

message(STATUS "WD01 vegetation source contract passed")
