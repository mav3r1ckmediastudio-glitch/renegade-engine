# Story Flow Gate 4C — Level Editor lifecycle proof.
add_executable(RenegadeStoryFlowGate4CLevelEditorLifecycleTests
    ${CMAKE_CURRENT_LIST_DIR}/StoryFlowGate4CLevelEditorLifecycleTests.cpp
)

target_link_libraries(
    RenegadeStoryFlowGate4CLevelEditorLifecycleTests
    PRIVATE
        Renegade::EngineBridge
)

target_compile_definitions(
    RenegadeStoryFlowGate4CLevelEditorLifecycleTests
    PRIVATE
        UNICODE
        _UNICODE
)

target_compile_options(
    RenegadeStoryFlowGate4CLevelEditorLifecycleTests
    PRIVATE
        "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>"
)

set_target_properties(
    RenegadeStoryFlowGate4CLevelEditorLifecycleTests
    PROPERTIES FOLDER "Renegade/Tests"
)

add_dependencies(
    RenegadeBridgeTests
    RenegadeStoryFlowGate4CLevelEditorLifecycleTests
)

add_test(
    NAME RenegadeStoryFlowGate4CLevelEditorLifecycleTests
    COMMAND RenegadeStoryFlowGate4CLevelEditorLifecycleTests
)
