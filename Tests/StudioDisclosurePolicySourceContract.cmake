set(chrome_header "${RENEGADE_SOURCE_DIR}/Studio/src/RenegadeStudioChrome.h")
set(chrome_source "${RENEGADE_SOURCE_DIR}/Studio/src/RenegadeStudioChrome.cpp")
set(inspector_source "${RENEGADE_SOURCE_DIR}/Studio/src/S1BInspectorSectionMigration.cpp")
set(studio_header "${RENEGADE_SOURCE_DIR}/Studio/src/StudioApplication.h")
set(studio_source "${RENEGADE_SOURCE_DIR}/Studio/src/StudioApplication.cpp")
set(audio_source "${RENEGADE_SOURCE_DIR}/Studio/src/RenegadePhysicsLabStudioChrome.cpp")

foreach(required_file IN ITEMS
    "${chrome_header}"
    "${chrome_source}"
    "${inspector_source}"
    "${studio_header}"
    "${studio_source}"
    "${audio_source}")
    if(NOT EXISTS "${required_file}")
        message(FATAL_ERROR "Studio disclosure policy file is missing: ${required_file}")
    endif()
endforeach()

file(READ "${chrome_header}" chrome_header_text)
file(READ "${chrome_source}" chrome_text)
file(READ "${inspector_source}" inspector_text)
file(READ "${studio_header}" studio_header_text)
file(READ "${studio_source}" studio_text)
file(READ "${audio_source}" audio_text)

foreach(token IN ITEMS
    "ResetDisclosureState"
    "ResetHierarchyDisclosureToSelection"
    "ResetAssetFolderDisclosureToSelection"
    "HasOpenMenuPopup")
    string(FIND "${chrome_header_text}" "${token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Studio disclosure chrome contract is missing ${token}")
    endif()
endforeach()

foreach(token IN ITEMS
    "collapsedHierarchyCategories_.fill(true)"
    "ResetHierarchyDisclosureToSelection();"
    "ResetAssetFolderDisclosureToSelection();")
    string(FIND "${chrome_text}" "${token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Studio disclosure implementation is missing ${token}")
    endif()
endforeach()

foreach(token IN ITEMS
    "MakeSection(\"transform\", \"TRANSFORM\", 10, false)"
    "MakeSection(\"rendering\", \"RENDERING\", 20, false)"
    "MakeSection(\"materials\", \"MATERIALS\", 30, false)"
    "ResetS1BInspectorDisclosure"
    "inspectorDisclosureSelectionRevision_"
    "inspectorDisclosureViewToken_")
    string(FIND "${inspector_text}${studio_header_text}" "${token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Studio Inspector disclosure policy is missing ${token}")
    endif()
endforeach()

foreach(token IN ITEMS
    "ResetS1BInspectorDisclosure();"
    "Action::ProjectHub"
    "Action::SceneWorkspace")
    string(FIND "${studio_text}" "${token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Studio view-reset seam is missing ${token}")
    endif()
endforeach()

foreach(token IN ITEMS
    "!HasOpenMenuPopup()"
    "if (HasOpenMenuPopup())"
    "ResetDisclosureState();")
    string(FIND "${audio_text}" "${token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Audio/menu ownership fix is missing ${token}")
    endif()
endforeach()

foreach(forbidden IN ITEMS "lua_State" "renegade.events" "GLOBAL SCRIPT")
    string(FIND "${chrome_text}${inspector_text}${audio_text}" "${forbidden}" found)
    if(NOT found EQUAL -1)
        message(FATAL_ERROR "Studio UI consistency gate must not introduce scripting/runtime work: ${forbidden}")
    endif()
endforeach()
