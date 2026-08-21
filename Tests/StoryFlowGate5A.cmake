# Story Flow Gate 5A — governed Screen creation / rollback proof.
add_executable(RenegadeStoryFlowGate5ScreenLifecycleTests
    ${CMAKE_CURRENT_LIST_DIR}/StoryFlowGate5ScreenLifecycleTests.cpp
)

target_link_libraries(
    RenegadeStoryFlowGate5ScreenLifecycleTests
    PRIVATE
        Renegade::EngineBridge
)

target_compile_options(
    RenegadeStoryFlowGate5ScreenLifecycleTests
    PRIVATE
        "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>"
)

set_target_properties(
    RenegadeStoryFlowGate5ScreenLifecycleTests
    PROPERTIES FOLDER "Renegade/Tests"
)

add_dependencies(
    RenegadeBridgeTests
    RenegadeStoryFlowGate5ScreenLifecycleTests
)

add_test(
    NAME RenegadeStoryFlowGate5ScreenLifecycleTests
    COMMAND RenegadeStoryFlowGate5ScreenLifecycleTests
)
