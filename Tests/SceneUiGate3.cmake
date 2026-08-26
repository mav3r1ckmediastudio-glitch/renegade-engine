# Scene UI Gate 3 — Inspector, Hierarchy and typography recovery contract.
add_test(
    NAME RenegadeSceneUiGate3SourceContract
    COMMAND ${CMAKE_COMMAND}
        -DRENEGADE_SOURCE_DIR=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_LIST_DIR}/SceneUiGate3SourceContract.cmake
)
