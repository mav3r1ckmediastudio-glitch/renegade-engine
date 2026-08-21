# Story Flow Gate 5C — stable Screen Editor handoff boundary proof.
add_executable(RenegadeStoryFlowGate5ScreenStudioBoundaryTests
    ${CMAKE_CURRENT_LIST_DIR}/StoryFlowGate5ScreenStudioBoundaryTests.cpp
)

target_link_libraries(
    RenegadeStoryFlowGate5ScreenStudioBoundaryTests
    PRIVATE
        Renegade::EngineBridge
)

target_include_directories(
    RenegadeStoryFlowGate5ScreenStudioBoundaryTests
    PRIVATE
        ${PROJECT_SOURCE_DIR}/Studio/src
)

target_compile_options(
    RenegadeStoryFlowGate5ScreenStudioBoundaryTests
    PRIVATE
        "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>"
)

set_target_properties(
    RenegadeStoryFlowGate5ScreenStudioBoundaryTests
    PROPERTIES FOLDER "Renegade/Tests"
)

add_dependencies(
    RenegadeBridgeTests
    RenegadeStoryFlowGate5ScreenStudioBoundaryTests
)

add_test(
    NAME RenegadeStoryFlowGate5ScreenStudioBoundaryTests
    COMMAND RenegadeStoryFlowGate5ScreenStudioBoundaryTests
)
