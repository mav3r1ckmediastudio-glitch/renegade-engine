# Scene UI Gate 5 — Environment and finite terrain recovery.
add_test(
    NAME RenegadeSceneUiGate5SourceContract
    COMMAND ${CMAKE_COMMAND}
        -DRENEGADE_SOURCE_DIR=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_LIST_DIR}/SceneUiGate5SourceContract.cmake
)
