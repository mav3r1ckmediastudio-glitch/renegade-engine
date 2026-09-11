# Phase 7A keeps animation authoring in Renegade Studio while EngineBridge owns
# the stable native Wicked AnimationComponent adapter.
target_sources(RenegadeStudio PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/src/Phase7AnimationInspector.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/Phase7AnimationInspector.h
)
