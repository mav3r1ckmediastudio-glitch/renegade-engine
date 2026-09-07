add_executable(RenegadeS7StockActionsRuntimeTests
    ${CMAKE_CURRENT_LIST_DIR}/S7StockActionsRuntimeTests.cpp
)

target_link_libraries(
    RenegadeS7StockActionsRuntimeTests
    PRIVATE
        Renegade::RuntimeBootstrap
)

target_compile_options(
    RenegadeS7StockActionsRuntimeTests
    PRIVATE
        "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>"
)

set_target_properties(
    RenegadeS7StockActionsRuntimeTests
    PROPERTIES
        FOLDER "Renegade/Tests"
)

add_executable(RenegadeS7StockLibraryRootsTests
    ${CMAKE_CURRENT_LIST_DIR}/S7StockLibraryRootsTests.cpp
)

target_link_libraries(
    RenegadeS7StockLibraryRootsTests
    PRIVATE
        Renegade::EngineBridge
)

target_compile_definitions(
    RenegadeS7StockLibraryRootsTests
    PRIVATE
        RENEGADE_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

target_compile_options(
    RenegadeS7StockLibraryRootsTests
    PRIVATE
        "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>"
)

set_target_properties(
    RenegadeS7StockLibraryRootsTests
    PROPERTIES
        FOLDER "Renegade/Tests"
)

add_dependencies(
    RenegadeBridgeTests
    RenegadeS7StockActionsRuntimeTests
    RenegadeS7StockLibraryRootsTests
)

add_test(
    NAME RenegadeS7StockActionsRuntimeTests
    COMMAND RenegadeS7StockActionsRuntimeTests
)

add_test(
    NAME RenegadeS7StockLibraryRootsTests
    COMMAND RenegadeS7StockLibraryRootsTests
)

add_test(
    NAME RenegadeS7StockActionsSourceContract
    COMMAND ${CMAKE_COMMAND}
        -DRENEGADE_SOURCE_DIR=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_LIST_DIR}/S7StockActionsSourceContract.cmake
)

set_tests_properties(
    RenegadeS7StockActionsRuntimeTests
    RenegadeS7StockLibraryRootsTests
    RenegadeS7StockActionsSourceContract
    PROPERTIES
        LABELS "S7"
)