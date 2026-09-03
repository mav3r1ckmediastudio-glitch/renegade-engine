set(framework_header "${RENEGADE_SOURCE_DIR}/Studio/src/InspectorSectionFramework.h")
set(framework_source "${RENEGADE_SOURCE_DIR}/Studio/src/InspectorSectionFramework.cpp")
set(studio_gate "${RENEGADE_SOURCE_DIR}/Studio/S1AInspectorSectionFramework.cmake")

foreach(required_file IN ITEMS
    "${framework_header}"
    "${framework_source}"
    "${studio_gate}")
    if(NOT EXISTS "${required_file}")
        message(FATAL_ERROR "S1A Inspector framework file is missing: ${required_file}")
    endif()
endforeach()

file(READ "${framework_header}" header_text)
file(READ "${framework_source}" source_text)
file(READ "${studio_gate}" gate_text)

foreach(required_symbol IN ITEMS
    "IInspectorSectionProvider"
    "InspectorSectionDescriptor"
    "InspectorSectionContext"
    "InspectorSectionLayout"
    "InspectorSectionRegistry"
    "ProjectInspectorSectionPreferenceStore"
    "LayoutVisibleSections"
    "RefreshVisibleSections"
    "SetExpanded"
    "ToggleExpanded")
    string(FIND "${header_text}" "${required_symbol}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "S1A Inspector framework contract is missing ${required_symbol}")
    endif()
endforeach()

string(FIND "${source_text}" "inspector.section." preference_prefix)
if(preference_prefix EQUAL -1)
    message(FATAL_ERROR "S1A expansion state is not persisted under the Inspector editor-preference namespace")
endif()

string(FIND "${source_text}" "GetEditorPreference" preference_read)
string(FIND "${source_text}" "SetEditorPreference" preference_write)
if(preference_read EQUAL -1 OR preference_write EQUAL -1)
    message(FATAL_ERROR "S1A ProjectService preference adapter is incomplete")
endif()

# S1A is deliberately presentation-framework only. Wicked widget ownership is
# introduced by S1B providers, not in the reusable registry itself.
string(FIND "${header_text}" "WickedEngine" wicked_header)
string(FIND "${source_text}" "WickedEngine" wicked_source)
if(NOT wicked_header EQUAL -1 OR NOT wicked_source EQUAL -1)
    message(FATAL_ERROR "S1A Inspector registry must remain independent of Wicked widget types")
endif()

string(FIND "${gate_text}" "RenegadeInspectorSectionFramework" framework_target)
string(FIND "${gate_text}" "target_link_libraries(RenegadeStudio" studio_link)
if(framework_target EQUAL -1 OR studio_link EQUAL -1)
    message(FATAL_ERROR "S1A Inspector framework is not wired into the Studio build graph")
endif()
