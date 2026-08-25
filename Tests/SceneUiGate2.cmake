# Scene UI Gate 2 — Scene shell, Story Flow return layering and specialist
# workspace isolation source contract.
add_test(
    NAME RenegadeSceneUiGate2SourceContract
    COMMAND ${CMAKE_COMMAND}
        -DRENEGADE_SOURCE_DIR=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_LIST_DIR}/SceneUiGate2SourceContract.cmake
)
