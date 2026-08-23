# Story Flow Gate 9C — visible/manipulable Journey routes over authoritative Flow history.
add_executable(RenegadeStoryFlowGate9CVisualRoutingTests
    ${CMAKE_CURRENT_LIST_DIR}/StoryFlowGate9CVisualRoutingTests.cpp
)

target_link_libraries(
    RenegadeStoryFlowGate9CVisualRoutingTests
    PRIVATE
        Renegade::EngineBridge
)

target_compile_options(
    RenegadeStoryFlowGate9CVisualRoutingTests
    PRIVATE
        "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>"
)

set_target_properties(
    RenegadeStoryFlowGate9CVisualRoutingTests
    PROPERTIES FOLDER "Renegade/Tests"
)

add_dependencies(
    RenegadeBridgeTests
    RenegadeStoryFlowGate9CVisualRoutingTests
)

add_test(
    NAME RenegadeStoryFlowGate9CVisualRoutingTests
    COMMAND RenegadeStoryFlowGate9CVisualRoutingTests
)
