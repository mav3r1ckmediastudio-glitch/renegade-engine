# Story Flow Gate 9D — single Graph ownership and Journey navigation contract.
add_test(
    NAME RenegadeStoryFlowGate9DSourceContract
    COMMAND ${CMAKE_COMMAND}
        -DRENEGADE_SOURCE_DIR=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_LIST_DIR}/StoryFlowGate9DSourceContract.cmake
)
