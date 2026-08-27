# Scene UI Gate 6 — consolidated whole-editor acceptance/hardening.
add_test(
    NAME RenegadeSceneUiGate6SourceContract
    COMMAND ${CMAKE_COMMAND}
        -DRENEGADE_SOURCE_DIR=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_LIST_DIR}/SceneUiGate6SourceContract.cmake
)
