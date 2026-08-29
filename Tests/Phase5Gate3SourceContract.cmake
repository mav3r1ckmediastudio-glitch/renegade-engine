if(NOT DEFINED RENEGADE_SOURCE_DIR)
    message(FATAL_ERROR "RENEGADE_SOURCE_DIR is required")
endif()

set(service_header "${RENEGADE_SOURCE_DIR}/EngineBridge/include/renegade/bridge/DecalProbeService.h")
set(service_source "${RENEGADE_SOURCE_DIR}/EngineBridge/src/DecalProbeService.cpp")
set(studio_source "${RENEGADE_SOURCE_DIR}/Studio/src/StudioApplication.cpp")
set(studio_header "${RENEGADE_SOURCE_DIR}/Studio/src/StudioApplication.h")
set(chrome_source "${RENEGADE_SOURCE_DIR}/Studio/src/RenegadeStudioChrome.cpp")

foreach(path IN ITEMS "${service_header}" "${service_source}")
    if(NOT EXISTS "${path}")
        message(FATAL_ERROR "Gate 3 missing required source: ${path}")
    endif()
endforeach()

file(READ "${service_header}" header_text)
file(READ "${service_source}" source_text)

foreach(token IN ITEMS
    "DecalState"
    "EnvironmentProbeState"
    "CreateDecalCommand"
    "SetDecalCommand"
    "CreateEnvironmentProbeCommand"
    "SetEnvironmentProbeCommand"
    "RefreshEnvironmentProbe")
    string(FIND "${header_text}" "${token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Gate 3 bridge contract missing ${token}")
    endif()
endforeach()

foreach(token IN ITEMS
    "Entity_CreateDecal"
    "Entity_CreateEnvironmentProbe"
    "SetBaseColorOnlyAlpha"
    "slopeBlendPower"
    "SetRealTime"
    "SetMSAA"
    "SetUpdateInterval"
    "view_distance"
    "SetDirty")
    string(FIND "${source_text}" "${token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Gate 3 native Wicked mapping missing ${token}")
    endif()
endforeach()

foreach(path IN ITEMS "${studio_source}" "${studio_header}" "${chrome_source}")
    if(NOT EXISTS "${path}")
        message(FATAL_ERROR "Gate 3 missing Studio source: ${path}")
    endif()
endforeach()
file(READ "${studio_source}" studio_text)
file(READ "${studio_header}" studio_header_text)
file(READ "${chrome_source}" chrome_text)
foreach(token IN ITEMS
    "DECAL // NATIVE WICKED"
    "ENVIRONMENT PROBE // NATIVE WICKED"
    "CreateDecalFromView"
    "CreateEnvironmentProbeFromView"
    "HandleDecalProbeSceneIcons"
    "RefreshEnvironmentProbe"
    "SetMaterialCommand")
    string(FIND "${studio_text}" "${token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Gate 3 Studio integration missing ${token}")
    endif()
endforeach()
foreach(token IN ITEMS
    "CreateDecal"
    "CreateEnvironmentProbe"
    "DecalProbeService.h")
    string(FIND "${studio_header_text}" "${token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Gate 3 Studio header missing ${token}")
    endif()
endforeach()
foreach(token IN ITEMS
    "DECAL"
    "ENVIRONMENT PROBE"
    "Action::CreateDecal"
    "Action::CreateEnvironmentProbe")
    string(FIND "${chrome_text}" "${token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Gate 3 ADD menu missing ${token}")
    endif()
endforeach()
