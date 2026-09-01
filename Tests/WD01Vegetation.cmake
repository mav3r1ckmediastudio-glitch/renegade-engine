add_test(
    NAME RenegadeWD01VegetationSourceContract
    COMMAND ${CMAKE_COMMAND}
        -DRENEGADE_SOURCE_DIR=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_SOURCE_DIR}/Tests/WD01VegetationSourceContract.cmake
)
