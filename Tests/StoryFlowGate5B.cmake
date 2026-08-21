# Story Flow Gate 5B — stable Screen resolution / authored outcome proof.
add_executable(RenegadeStoryFlowGate5ScreenReferenceTests
    ${CMAKE_CURRENT_LIST_DIR}/StoryFlowGate5ScreenReferenceTests.cpp
)

target_link_libraries(
    RenegadeStoryFlowGate5ScreenReferenceTests
    PRIVATE
        Renegade::EngineBridge
)

target_compile_options(
    RenegadeStoryFlowGate5ScreenReferenceTests
    PRIVATE
        "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>"
)

set_target_properties(
    RenegadeStoryFlowGate5ScreenReferenceTests
    PROPERTIES FOLDER "Renegade/Tests"
)

add_dependencies(
    RenegadeBridgeTests
    RenegadeStoryFlowGate5ScreenReferenceTests
)

add_test(
    NAME RenegadeStoryFlowGate5ScreenReferenceTests
    COMMAND RenegadeStoryFlowGate5ScreenReferenceTests
)
