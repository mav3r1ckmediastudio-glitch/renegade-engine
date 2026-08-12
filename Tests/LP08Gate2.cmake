add_executable(RenegadeResourceAssetTests
    ${CMAKE_CURRENT_LIST_DIR}/ResourceAssetTests.cpp
)

target_link_libraries(
    RenegadeResourceAssetTests
    PRIVATE
        Renegade::EngineBridge
)

target_compile_options(
    RenegadeResourceAssetTests
    PRIVATE
        "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>"
)

set_target_properties(
    RenegadeResourceAssetTests
    PROPERTIES
        FOLDER "Renegade/Tests"
)

add_executable(RenegadeResourceAssetRollbackTests
    ${CMAKE_CURRENT_LIST_DIR}/ResourceAssetRollbackTests.cpp
)

target_link_libraries(
    RenegadeResourceAssetRollbackTests
    PRIVATE
        Renegade::EngineBridge
)

target_compile_options(
    RenegadeResourceAssetRollbackTests
    PRIVATE
        "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>"
)

set_target_properties(
    RenegadeResourceAssetRollbackTests
    PROPERTIES
        FOLDER "Renegade/Tests"
)

# Studio CI explicitly builds RenegadeBridgeTests before running the full CTest
# suite. Gate 2 is headless and must execute in both Debug and Release.
add_dependencies(
    RenegadeBridgeTests
    RenegadeResourceAssetTests
    RenegadeResourceAssetRollbackTests
)

add_test(
    NAME RenegadeResourceAssetTests
    COMMAND RenegadeResourceAssetTests
)

add_test(
    NAME RenegadeResourceAssetRollbackTests
    COMMAND RenegadeResourceAssetRollbackTests
)
