add_test(
    NAME RenegadeEditorFrameLoopRecoverySourceContract
    COMMAND ${CMAKE_COMMAND}
        -DRENEGADE_SOURCE_DIR=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_LIST_DIR}/EditorFrameLoopRecoverySourceContract.cmake
)
