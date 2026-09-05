# Story Flow Gate 4D — integrated Level lifecycle acceptance proof.
add_executable(RenegadeStoryFlowGate4DAcceptanceTests
    ${CMAKE_CURRENT_LIST_DIR}/StoryFlowGate4DAcceptanceTests.cpp
)

target_link_libraries(
    RenegadeStoryFlowGate4DAcceptanceTests
    PRIVATE
        Renegade::EngineBridge
)

target_compile_definitions(
    RenegadeStoryFlowGate4DAcceptanceTests
    PRIVATE
        UNICODE
        _UNICODE
)

target_compile_options(
    RenegadeStoryFlowGate4DAcceptanceTests
    PRIVATE
        "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>"
)

set_target_properties(
    RenegadeStoryFlowGate4DAcceptanceTests
    PROPERTIES FOLDER "Renegade/Tests"
)

add_dependencies(
    RenegadeBridgeTests
    RenegadeStoryFlowGate4DAcceptanceTests
)

add_test(
    NAME RenegadeStoryFlowGate4DAcceptanceTests
    COMMAND RenegadeStoryFlowGate4DAcceptanceTests
)
