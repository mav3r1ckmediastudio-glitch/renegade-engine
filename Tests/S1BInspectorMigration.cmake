add_test(
    NAME RenegadeS1BInspectorMigrationSourceContract
    COMMAND ${CMAKE_COMMAND}
        -DRENEGADE_SOURCE_DIR=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_LIST_DIR}/S1BInspectorMigrationSourceContract.cmake
)
