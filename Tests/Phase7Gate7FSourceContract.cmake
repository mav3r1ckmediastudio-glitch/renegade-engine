if(NOT DEFINED RENEGADE_SOURCE_DIR)
    message(FATAL_ERROR "RENEGADE_SOURCE_DIR is required")
endif()

function(require_file path label)
    if(NOT EXISTS "${path}")
        message(FATAL_ERROR "Phase 7F missing ${label}: ${path}")
    endif()
endfunction()

function(require_text text needle label)
    string(FIND "${text}" "${needle}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "Phase 7F source contract missing ${label}: ${needle}")
    endif()
endfunction()

function(reject_text text needle label)
    string(FIND "${text}" "${needle}" position)
    if(NOT position EQUAL -1)
        message(FATAL_ERROR "Phase 7F source contract rejects ${label}: ${needle}")
    endif()
endfunction()

set(render_header "${RENEGADE_SOURCE_DIR}/EngineBridge/include/renegade/bridge/RenderSettingsService.h")
set(render_source "${RENEGADE_SOURCE_DIR}/EngineBridge/src/RenderSettingsService.cpp")
set(material_source "${RENEGADE_SOURCE_DIR}/EngineBridge/src/MaterialService.cpp")
set(inspector_source "${RENEGADE_SOURCE_DIR}/Studio/src/Phase7Gate7FMeshBlendInspector.cpp")
set(animation_header "${RENEGADE_SOURCE_DIR}/Studio/src/Phase7Gate7AAnimationInspector.h")
set(root_cmake "${RENEGADE_SOURCE_DIR}/CMakeLists.txt")
set(test_source "${RENEGADE_SOURCE_DIR}/Tests/Phase7Gate7FMeshBlendTests.cpp")

foreach(file IN ITEMS
    "${render_header}"
    "${render_source}"
    "${material_source}"
    "${inspector_source}"
    "${animation_header}"
    "${root_cmake}"
    "${test_source}")
    require_file("${file}" "required source")
endforeach()

file(READ "${render_header}" render_header_text)
file(READ "${render_source}" render_source_text)
file(READ "${material_source}" material_source_text)
file(READ "${inspector_source}" inspector)
file(READ "${animation_header}" animation)
file(READ "${root_cmake}" root)
file(READ "${test_source}" tests)

# Keep Gate 9's accepted persistence contract: 7F is an optional schema-v3
# field with Wicked's enabled default, not a gratuitous schema migration.
require_text("${render_header_text}" "RenderSettingsSchemaVersion = 3" "unchanged render schema")
require_text("${render_header_text}" "bool meshBlendingEnabled = true" "native enabled default")
require_text("${render_source_text}" "renegade.render.mesh_blending.enabled" "persisted mesh-blend key")
require_text("${render_source_text}" "getMeshBlendEnabled()" "native RenderPath readback")
require_text("${render_source_text}" "setMeshBlendEnabled(safe.meshBlendingEnabled)" "native RenderPath application")
require_text("${render_source_text}" "meshBlendingEnabled != right.meshBlendingEnabled" "render-state dirty comparison")

# Material blending must remain native MaterialComponent state and must not be
# reclassified as a terrain-only shader feature.
require_text("${material_source_text}" "state.meshBlend = material.mesh_blend" "native material capture")
require_text("${material_source_text}" "material.mesh_blend = safe.meshBlend" "native material apply")
require_text("${inspector}" "CollectEditableMaterialEntities" "ordinary material discovery")
require_text("${inspector}" "SetMaterialCommand" "command-backed material authoring")
require_text("${inspector}" "MESH BLEND // MATERIAL" "creator mesh-blend control")
require_text("${inspector}" "Global mesh blending" "global creator control")
reject_text("${inspector}" "SHADERTYPE_PBR_TERRAINBLENDED" "terrain-only mesh-blend gate")

require_text("${animation}" "RegisterPhase7Gate7FMeshBlendInspector" "7F inspector registration")
require_text("${animation}" "PreparePhase7Gate7FMeshBlendInspector" "7F inspector layout preparation")
require_text("${root}" "Phase7Gate7FMeshBlendInspector.cpp" "7F Studio target ownership")
require_text("${root}" "include(Tests/Phase7Gate7F.cmake)" "7F test registration")

require_text("${tests}" "Material::SHADERTYPE_PBR" "ordinary PBR regression")
require_text("${tests}" "getMeshBlendEnabled" "native global toggle regression")
require_text("${tests}" "RenderSettingsSchemaVersion != 3" "schema preservation regression")

foreach(stock_header IN ITEMS
    "MaterialWindow.h"
    "GraphicsWindow.h"
    "EditorComponent.h")
    reject_text("${inspector}" "${stock_header}" "stock Wicked editor embedding")
endforeach()

message(STATUS "Phase 7F native mesh blending parity source contract passed")
