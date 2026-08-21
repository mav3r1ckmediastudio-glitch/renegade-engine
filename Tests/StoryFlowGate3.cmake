# Story Flow Gate 3 — semantic authoring session/history proof.
add_executable(RenegadeStoryFlowAuthoringSessionTests
    ${CMAKE_CURRENT_LIST_DIR}/StoryFlowAuthoringSessionTests.cpp
)

target_link_libraries(
    RenegadeStoryFlowAuthoringSessionTests
    PRIVATE
        Renegade::EngineBridge
)

target_compile_options(
    RenegadeStoryFlowAuthoringSessionTests
    PRIVATE
        "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>"
)

set_target_properties(
    RenegadeStoryFlowAuthoringSessionTests
    PROPERTIES FOLDER "Renegade/Tests"
)

add_dependencies(
    RenegadeBridgeTests
    RenegadeStoryFlowAuthoringSessionTests
)

add_test(
    NAME RenegadeStoryFlowAuthoringSessionTests
    COMMAND RenegadeStoryFlowAuthoringSessionTests
)
