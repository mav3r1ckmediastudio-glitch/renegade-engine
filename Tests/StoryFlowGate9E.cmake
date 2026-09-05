# Story Flow Gate 9E — authoritative nonlinear Journey authoring proof.
add_executable(RenegadeStoryFlowGate9ENonlinearAuthoringTests
    ${CMAKE_CURRENT_LIST_DIR}/StoryFlowGate9ENonlinearAuthoringTests.cpp
)

target_link_libraries(
    RenegadeStoryFlowGate9ENonlinearAuthoringTests
    PRIVATE
        Renegade::EngineBridge
)

target_compile_options(
    RenegadeStoryFlowGate9ENonlinearAuthoringTests
    PRIVATE
        "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>"
)

set_target_properties(
    RenegadeStoryFlowGate9ENonlinearAuthoringTests
    PROPERTIES FOLDER "Renegade/Tests"
)

add_dependencies(
    RenegadeBridgeTests
    RenegadeStoryFlowGate9ENonlinearAuthoringTests
)

add_test(
    NAME RenegadeStoryFlowGate9ENonlinearAuthoringTests
    COMMAND RenegadeStoryFlowGate9ENonlinearAuthoringTests
)
