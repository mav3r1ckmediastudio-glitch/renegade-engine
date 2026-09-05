# Story Flow Gate 5 recovery — Project Home Create/Open/Recent proof.
add_executable(RenegadeStoryFlowGate5ProjectHomeRecoveryTests
    ${CMAKE_CURRENT_LIST_DIR}/StoryFlowGate5ProjectHomeRecoveryTests.cpp
)

target_link_libraries(
    RenegadeStoryFlowGate5ProjectHomeRecoveryTests
    PRIVATE
        Renegade::EngineBridge
)

target_compile_definitions(
    RenegadeStoryFlowGate5ProjectHomeRecoveryTests
    PRIVATE
        UNICODE
        _UNICODE
)

target_compile_options(
    RenegadeStoryFlowGate5ProjectHomeRecoveryTests
    PRIVATE
        "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>"
)

set_target_properties(
    RenegadeStoryFlowGate5ProjectHomeRecoveryTests
    PROPERTIES FOLDER "Renegade/Tests"
)

add_dependencies(
    RenegadeBridgeTests
    RenegadeStoryFlowGate5ProjectHomeRecoveryTests
)

add_test(
    NAME RenegadeStoryFlowGate5ProjectHomeRecoveryTests
    COMMAND RenegadeStoryFlowGate5ProjectHomeRecoveryTests
)
