# Story Flow Journey recovery 9A — lifecycle presentation replacement contract.
add_test(
    NAME RenegadeStoryFlowJourneyRecovery9ASourceContract
    COMMAND ${CMAKE_COMMAND}
        -DRENEGADE_SOURCE_DIR=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_LIST_DIR}/StoryFlowJourneyRecovery9ASourceContract.cmake
)

add_executable(RenegadeStoryFlowJourneyUiLayoutTests
    ${CMAKE_CURRENT_LIST_DIR}/StoryFlowJourneyUiLayoutTests.cpp
)

target_include_directories(RenegadeStoryFlowJourneyUiLayoutTests
    PRIVATE ${CMAKE_SOURCE_DIR}/Studio/src
)

target_compile_options(RenegadeStoryFlowJourneyUiLayoutTests
    PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>"
)

set_target_properties(RenegadeStoryFlowJourneyUiLayoutTests
    PROPERTIES FOLDER "Renegade/Tests"
)

add_dependencies(RenegadeBridgeTests
    RenegadeStoryFlowJourneyUiLayoutTests
)

add_test(
    NAME RenegadeStoryFlowJourneyUiLayoutTests
    COMMAND RenegadeStoryFlowJourneyUiLayoutTests
)
