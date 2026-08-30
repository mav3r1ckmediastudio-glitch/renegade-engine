# Phase 5 Gate 8 keeps static-lighting authoring in Renegade EngineBridge while
# reusing Wicked's pinned xatlas utility implementation. This does not enable
# or link the stock Wicked Editor application/UI.
target_sources(RenegadeEngineBridge PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/src/LightmapBakeService.cpp
    ${CMAKE_CURRENT_LIST_DIR}/include/renegade/bridge/LightmapBakeService.h
    ${PROJECT_SOURCE_DIR}/WickedEngine/Editor/xatlas.cpp
)
