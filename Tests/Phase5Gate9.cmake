add_test(
    NAME RenegadePhase5Gate9SourceContract
    COMMAND ${CMAKE_COMMAND}
        -DRENEGADE_SOURCE_DIR=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_SOURCE_DIR}/Tests/Phase5Gate9SourceContract.cmake
)
