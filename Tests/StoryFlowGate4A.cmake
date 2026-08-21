# Story Flow Gate 4A — governed Level creation / rollback proof.
add_executable(RenegadeStoryFlowGate4LevelLifecycleTests
    ${CMAKE_CURRENT_LIST_DIR}/StoryFlowGate4LevelLifecycleTests.cpp
)

target_link_libraries(
    RenegadeStoryFlowGate4LevelLifecycleTests
    PRIVATE
        Renegade::EngineBridge
)

target_compile_options(
    RenegadeStoryFlowGate4LevelLifecycleTests
    PRIVATE
        "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>"
)

set_target_properties(
    RenegadeStoryFlowGate4LevelLifecycleTests
    PROPERTIES FOLDER "Renegade/Tests"
)

add_dependencies(
    RenegadeBridgeTests
    RenegadeStoryFlowGate4LevelLifecycleTests
)

add_test(
    NAME RenegadeStoryFlowGate4LevelLifecycleTests
    COMMAND RenegadeStoryFlowGate4LevelLifecycleTests
)
