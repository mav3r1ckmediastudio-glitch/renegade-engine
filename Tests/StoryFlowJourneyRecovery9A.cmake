# Story Flow Journey recovery 9A — lifecycle presentation replacement contract.
add_test(
    NAME RenegadeStoryFlowJourneyRecovery9ASourceContract
    COMMAND ${CMAKE_COMMAND}
        -DRENEGADE_SOURCE_DIR=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_LIST_DIR}/StoryFlowJourneyRecovery9ASourceContract.cmake
)
