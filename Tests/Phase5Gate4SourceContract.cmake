if(NOT DEFINED RENEGADE_SOURCE_DIR)
    message(FATAL_ERROR "RENEGADE_SOURCE_DIR is required")
endif()

set(material_header "${RENEGADE_SOURCE_DIR}/EngineBridge/include/renegade/bridge/MaterialService.h")
set(material_source "${RENEGADE_SOURCE_DIR}/EngineBridge/src/MaterialService.cpp")
set(texture_header "${RENEGADE_SOURCE_DIR}/EngineBridge/include/renegade/bridge/MaterialTextureAssetService.h")
set(texture_source "${RENEGADE_SOURCE_DIR}/EngineBridge/src/MaterialTextureAssetService.cpp")
set(texture_clear "${RENEGADE_SOURCE_DIR}/EngineBridge/src/MaterialTextureClearCommand.cpp")
set(bridge_cmake "${RENEGADE_SOURCE_DIR}/EngineBridge/CMakeLists.txt")
set(studio_header "${RENEGADE_SOURCE_DIR}/Studio/src/StudioApplication.h")
set(studio_source "${RENEGADE_SOURCE_DIR}/Studio/src/StudioApplication.cpp")
set(studio_material_source "${RENEGADE_SOURCE_DIR}/Studio/src/Phase5Gate4MaterialInspector.cpp")
set(studio_cmake "${RENEGADE_SOURCE_DIR}/Studio/CMakeLists.txt")
set(gate_test "${RENEGADE_SOURCE_DIR}/Tests/Phase5Gate4MaterialTests.cpp")
set(gate_doc "${RENEGADE_SOURCE_DIR}/docs/PHASE5_GATE4_MATERIAL_SHADER.md")

foreach(path IN ITEMS
    "${material_header}"
    "${material_source}"
    "${texture_header}"
    "${texture_source}"
    "${texture_clear}"
    "${bridge_cmake}"
    "${studio_header}"
    "${studio_source}"
    "${studio_material_source}"
    "${studio_cmake}"
    "${gate_test}"
    "${gate_doc}")
    if(NOT EXISTS "${path}")
        message(FATAL_ERROR "Gate 4 missing required source: ${path}")
    endif()
endforeach()

file(READ "${material_header}" header_text)
file(READ "${material_source}" source_text)
file(READ "${texture_header}" texture_header_text)
file(READ "${texture_source}" texture_source_text)
file(READ "${texture_clear}" texture_clear_text)
file(READ "${bridge_cmake}" bridge_cmake_text)
file(READ "${studio_header}" studio_header_text)
file(READ "${studio_source}" studio_source_text)
file(READ "${studio_material_source}" studio_material_text)
file(READ "${studio_cmake}" studio_cmake_text)
file(READ "${gate_test}" test_text)

foreach(token IN ITEMS
    "CollectEditableMaterialEntities"
    "shaderType"
    "blendMode"
    "normalMapStrength"
    "alphaRef"
    "receiveShadow"
    "castShadow"
    "useVertexColors"
    "doubleSided"
    "texMulAdd"
    "parallaxOcclusionMapping"
    "anisotropyStrength"
    "anisotropyRotationDegrees"
    "sheenColor"
    "sheenRoughness"
    "clearcoat"
    "clearcoatRoughness"
    "transmission"
    "refraction"
    "blendWithTerrainHeight"
    "meshBlend"
    "interiorMappingScale"
    "interiorMappingOffset"
    "interiorMappingRotationDegrees")
    string(FIND "${header_text}" "${token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Gate 4 material contract missing ${token}")
    endif()
endforeach()

foreach(token IN ITEMS
    "SHADERTYPE_PBR_PLANARREFLECTION"
    "SHADERTYPE_PBR_PARALLAXOCCLUSIONMAPPING"
    "SHADERTYPE_PBR_ANISOTROPIC"
    "SHADERTYPE_PBR_CLOTH"
    "SHADERTYPE_PBR_CLEARCOAT"
    "SHADERTYPE_PBR_CLOTH_CLEARCOAT"
    "SHADERTYPE_PBR_TERRAINBLENDED"
    "SHADERTYPE_WATER"
    "SHADERTYPE_CARTOON"
    "SHADERTYPE_UNLIT"
    "SHADERTYPE_INTERIORMAPPING"
    "BLENDMODE_ALPHA"
    "BLENDMODE_PREMULTIPLIED"
    "BLENDMODE_ADDITIVE"
    "BLENDMODE_MULTIPLY"
    "BLENDMODE_INVERSE"
    "SetNormalMapStrength"
    "SetAlphaRef"
    "SetReceiveShadow"
    "SetCastShadow"
    "SetUseVertexColors"
    "SetDoubleSided"
    "SetParallaxOcclusionMapping"
    "SetTransmissionAmount"
    "SetRefractionAmount"
    "SetInteriorMappingScale"
    "SetInteriorMappingOffset"
    "SetInteriorMappingRotation")
    string(FIND "${source_text}" "${token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Gate 4 native Wicked mapping missing ${token}")
    endif()
endforeach()

foreach(token IN ITEMS
    "enum class MaterialTextureSlot"
    "ClearMaterialTextureAssetCommand")
    string(FIND "${texture_header_text}" "${token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Gate 4 governed texture contract missing ${token}")
    endif()
endforeach()

foreach(token IN ITEMS
    "MaterialTextureSlot::BaseColor"
    "MaterialTextureSlot::Normal"
    "MaterialTextureSlot::Surface"
    "MaterialTextureSlot::Emissive"
    "MaterialTextureSlot::Occlusion"
    "wi::scene::MaterialComponent::BASECOLORMAP"
    "wi::scene::MaterialComponent::NORMALMAP"
    "wi::scene::MaterialComponent::SURFACEMAP"
    "wi::scene::MaterialComponent::EMISSIVEMAP"
    "wi::scene::MaterialComponent::OCCLUSIONMAP")
    string(FIND "${texture_source_text}" "${token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Gate 4 governed texture mapping missing ${token}")
    endif()
endforeach()

foreach(token IN ITEMS
    "WickedTextureSlot(slot_)"
    "MaterialTextureSlotMetadataKey(slot_)"
    "MaterialTextureAssetBindingVersionMetadataKey"
    "MaterialBaseColorTextureAssetIdMetadataKey"
    "MaterialNormalTextureAssetIdMetadataKey"
    "MaterialSurfaceTextureAssetIdMetadataKey"
    "MaterialEmissiveTextureAssetIdMetadataKey"
    "MaterialOcclusionTextureAssetIdMetadataKey")
    string(FIND "${texture_clear_text}" "${token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Gate 4 texture clear mapping missing ${token}")
    endif()
endforeach()

string(FIND "${bridge_cmake_text}" "src/MaterialTextureClearCommand.cpp" clear_wired)
if(clear_wired EQUAL -1)
    message(FATAL_ERROR "Gate 4 texture clear command is not wired into EngineBridge")
endif()

foreach(token IN ITEMS
    "CollectEditableMaterialEntities"
    "SetMaterialCommand"
    "ClearMaterialTextureAssetCommand"
    "SHADERTYPE_PBR_CLEARCOAT"
    "BLENDMODE_ALPHA")
    string(FIND "${test_text}" "${token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Gate 4 regression missing ${token}")
    endif()
endforeach()

# Creator-facing Studio acceptance surface. Keep this separate from the bridge
# checks above so Gate 4 cannot regress to a backend-only material capability.
foreach(token IN ITEMS
    "CreateMaterialInspector"
    "RefreshMaterialInspector"
    "CommitSelectedMaterial"
    "ChooseSelectedMaterialTexture"
    "ClearSelectedMaterialTexture"
    "materialInspectorEntity_"
    "materialTextureImportWorkload_")
    string(FIND "${studio_header_text}" "${token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Gate 4 Studio declaration missing ${token}")
    endif()
endforeach()

foreach(token IN ITEMS
    "CreateMaterialInspector();"
    "RefreshMaterialInspector("
    "LayoutMaterialInspector("
    "materialInspectorVisible_")
    string(FIND "${studio_source_text}" "${token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Gate 4 Studio integration hook missing ${token}")
    endif()
endforeach()

foreach(token IN ITEMS
    "MATERIAL // NATIVE WICKED"
    "PBR // CORE PROPERTIES"
    "UV // TEXTURE TRANSFORM"
    "TEXTURES // GOVERNED PROJECT ASSETS"
    "SHADER // NATIVE PARAMETERS"
    "CollectEditableMaterialEntities"
    "SetMaterialCommand"
    "SetMaterialTextureAssetCommand"
    "ClearMaterialTextureAssetCommand"
    "MaterialTextureSlot::BaseColor"
    "MaterialTextureSlot::Normal"
    "MaterialTextureSlot::Surface"
    "MaterialTextureSlot::Emissive"
    "MaterialTextureSlot::Occlusion"
    "PACKED SURFACE"
    "SHADERTYPE_PBR_PARALLAXOCCLUSIONMAPPING"
    "SHADERTYPE_PBR_ANISOTROPIC"
    "SHADERTYPE_PBR_CLEARCOAT"
    "SHADERTYPE_PBR_CLOTH_CLEARCOAT"
    "SHADERTYPE_PBR_TERRAINBLENDED"
    "SHADERTYPE_WATER"
    "SHADERTYPE_INTERIORMAPPING"
    "CreatorTextureWorkflowService"
    "PrepareMaterialTextureAsset")
    string(FIND "${studio_material_text}" "${token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Gate 4 Studio material Inspector missing ${token}")
    endif()
endforeach()

string(FIND "${studio_cmake_text}" "src/Phase5Gate4MaterialInspector.cpp" material_ui_wired)
if(material_ui_wired EQUAL -1)
    message(FATAL_ERROR "Gate 4 material Inspector is not wired into RenegadeStudio")
endif()
