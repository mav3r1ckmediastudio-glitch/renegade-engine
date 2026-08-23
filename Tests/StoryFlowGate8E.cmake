# Story Flow Gate 8E — Screen action / Story Flow outcome parity.
add_executable(RenegadeStoryFlowGate8EOutcomeParityTests
    ${CMAKE_CURRENT_LIST_DIR}/StoryFlowGate8EOutcomeParityTests.cpp
)

target_link_libraries(
    RenegadeStoryFlowGate8EOutcomeParityTests
    PRIVATE
        Renegade::EngineBridge
        Renegade::RuntimeBootstrap
)

target_compile_options(
    RenegadeStoryFlowGate8EOutcomeParityTests
    PRIVATE
        "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>"
)

set_target_properties(
    RenegadeStoryFlowGate8EOutcomeParityTests
    PROPERTIES FOLDER "Renegade/Tests"
)

add_dependencies(
    RenegadeBridgeTests
    RenegadeStoryFlowGate8EOutcomeParityTests
)

add_test(
    NAME RenegadeStoryFlowGate8EOutcomeParityTests
    COMMAND RenegadeStoryFlowGate8EOutcomeParityTests
)
