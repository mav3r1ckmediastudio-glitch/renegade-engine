set(studio_header "${RENEGADE_SOURCE_DIR}/Studio/src/StudioApplication.h")
set(studio_source "${RENEGADE_SOURCE_DIR}/Studio/src/StudioApplication.cpp")
set(migration_source "${RENEGADE_SOURCE_DIR}/Studio/src/S1BInspectorSectionMigration.cpp")
set(material_source "${RENEGADE_SOURCE_DIR}/Studio/src/Phase5Gate4MaterialInspector.cpp")
set(studio_gate "${RENEGADE_SOURCE_DIR}/Studio/S1BInspectorMigration.cmake")

foreach(required_file IN ITEMS
    "${studio_header}"
    "${studio_source}"
    "${migration_source}"
    "${material_source}"
    "${studio_gate}")
    if(NOT EXISTS "${required_file}")
        message(FATAL_ERROR "S1B Inspector migration file is missing: ${required_file}")
    endif()
endforeach()

file(READ "${studio_header}" header_text)
file(READ "${studio_source}" source_text)
file(READ "${migration_source}" migration_text)
file(READ "${material_source}" material_text)
file(READ "${studio_gate}" gate_text)

foreach(token IN ITEMS
    "InspectorSectionFramework.h"
    "inspectorSectionRegistry_"
    "transformSectionHeader_"
    "renderingSectionHeader_"
    "materialsSectionHeader_"
    "CreateS1BInspectorSections"
    "LayoutS1BInspectorSections")
    string(FIND "${header_text}" "${token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "S1B Studio header integration is missing ${token}")
    endif()
endforeach()

foreach(token IN ITEMS
    "MakeSection(\"transform\""
    "MakeSection(\"rendering\""
    "MakeSection(\"materials\""
    "ProjectInspectorSectionPreferenceStore"
    "LayoutVisibleSections"
    "RefreshVisibleSections"
    "ToggleExpanded"
    "LayoutMaterialInspector"
    "InspectObjectParticipation")
    string(FIND "${migration_text}" "${token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "S1B migration contract is missing ${token}")
    endif()
endforeach()

foreach(token IN ITEMS
    "CreateMaterialInspector();"
    "CreateS1BInspectorSections();"
    "LayoutS1BInspectorSections();")
    string(FIND "${source_text}" "${token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "S1B StudioApplication seam is missing ${token}")
    endif()
endforeach()

# The specialist material implementation remains authoritative. S1B wraps its
# layout/visibility instead of cloning or replacing the Gate 4 material editor.
foreach(token IN ITEMS
    "StudioRenderPath::LayoutMaterialInspector"
    "StudioRenderPath::RefreshMaterialInspector"
    "CollectEditableMaterialEntities")
    string(FIND "${material_text}" "${token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "S1B regressed the specialist material path: ${token}")
    endif()
endforeach()

string(FIND "${gate_text}" "S1BInspectorSectionMigration.cpp" wired)
if(wired EQUAL -1)
    message(FATAL_ERROR "S1B migration source is not wired into RenegadeStudio")
endif()

foreach(forbidden IN ITEMS "lua_State" "renegade.events" "ACTION //" "GLOBAL SCRIPT")
    string(FIND "${migration_text}" "${forbidden}" found)
    if(NOT found EQUAL -1)
        message(FATAL_ERROR "S1B must not introduce scripting/runtime work: ${forbidden}")
    endif()
endforeach()
