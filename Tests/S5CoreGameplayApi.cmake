add_executable(RenegadeS5CoreGameplayApiTests
    ${CMAKE_CURRENT_LIST_DIR}/S5CoreGameplayApiTests.cpp
)

target_link_libraries(
    RenegadeS5CoreGameplayApiTests
    PRIVATE
        Renegade::RuntimeBootstrap
)

target_compile_options(
    RenegadeS5CoreGameplayApiTests
    PRIVATE
        "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>"
)

set_target_properties(
    RenegadeS5CoreGameplayApiTests
    PROPERTIES
        FOLDER "Renegade/Tests"
)

add_dependencies(
    RenegadeBridgeTests
    RenegadeS5CoreGameplayApiTests
)

add_test(
    NAME RenegadeS5CoreGameplayApiTests
    COMMAND RenegadeS5CoreGameplayApiTests
)

add_test(
    NAME RenegadeS5CoreGameplayApiSourceContract
    COMMAND ${CMAKE_COMMAND}
        -DRENEGADE_SOURCE_DIR=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_LIST_DIR}/S5CoreGameplayApiSourceContract.cmake
)
