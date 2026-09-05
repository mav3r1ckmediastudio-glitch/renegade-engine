# Scene UI Gate 4 — Asset Browser, placement and shared-control recovery.
add_test(
    NAME RenegadeSceneUiGate4SourceContract
    COMMAND ${CMAKE_COMMAND}
        -DRENEGADE_SOURCE_DIR=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_LIST_DIR}/SceneUiGate4SourceContract.cmake
)
