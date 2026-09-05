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

add_executable(RenegadeS5CoreGameplayLuaTests
    ${CMAKE_CURRENT_LIST_DIR}/S5CoreGameplayLuaTests.cpp
)

target_link_libraries(
    RenegadeS5CoreGameplayLuaTests
    PRIVATE
        Renegade::RuntimeBootstrap
)

target_compile_options(
    RenegadeS5CoreGameplayLuaTests
    PRIVATE
        "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>"
)

set_target_properties(
    RenegadeS5CoreGameplayLuaTests
    PROPERTIES
        FOLDER "Renegade/Tests"
)

add_dependencies(
    RenegadeBridgeTests
    RenegadeS5CoreGameplayLuaTests
)

add_test(
    NAME RenegadeS5CoreGameplayLuaTests
    COMMAND RenegadeS5CoreGameplayLuaTests
)

add_executable(RenegadeS5ReusableScriptOwnerTests
    ${CMAKE_CURRENT_LIST_DIR}/S5ReusableScriptOwnerRegression.cpp
)

target_link_libraries(
    RenegadeS5ReusableScriptOwnerTests
    PRIVATE
        Renegade::RuntimeBootstrap
)

target_compile_options(
    RenegadeS5ReusableScriptOwnerTests
    PRIVATE
        "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>"
)

set_target_properties(
    RenegadeS5ReusableScriptOwnerTests
    PROPERTIES
        FOLDER "Renegade/Tests"
)

# The fast S5A workflow already builds RenegadeS5CoreGameplayLuaTests.
# Make the reusable-owner regression part of that target graph so no
# additional full Studio build is needed for preflight.
add_dependencies(
    RenegadeS5CoreGameplayLuaTests
    RenegadeS5ReusableScriptOwnerTests
)
add_dependencies(
    RenegadeBridgeTests
    RenegadeS5ReusableScriptOwnerTests
)

add_test(
    NAME RenegadeS5CoreGameplayLuaTestsReusableOwner
    COMMAND RenegadeS5ReusableScriptOwnerTests
)

add_test(
    NAME RenegadeS5CoreGameplayApiSourceContract
    COMMAND ${CMAKE_COMMAND}
        -DRENEGADE_SOURCE_DIR=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_LIST_DIR}/S5CoreGameplayApiSourceContract.cmake
)

# S5B Gate 4: the full governed gameplay lifecycle seam. This test exercises
# lifecycle callbacks together with the live PlayerService/InputService state.
add_executable(RenegadeS5BGameplayLifecycleTests
    ${CMAKE_CURRENT_LIST_DIR}/S5BGameplayLifecycleTests.cpp
)

target_link_libraries(
    RenegadeS5BGameplayLifecycleTests
    PRIVATE
        Renegade::RuntimeBootstrap
)

target_compile_options(
    RenegadeS5BGameplayLifecycleTests
    PRIVATE
        "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>"
)

set_target_properties(
    RenegadeS5BGameplayLifecycleTests
    PROPERTIES
        FOLDER "Renegade/Tests"
)

add_dependencies(
    RenegadeBridgeTests
    RenegadeS5BGameplayLifecycleTests
)

add_test(
    NAME RenegadeS5BGameplayLifecycleTests
    COMMAND RenegadeS5BGameplayLifecycleTests
)

# S5C Gate: bounded cross-script event queue contract.
add_executable(RenegadeS5CGameplayEventTests
    ${CMAKE_CURRENT_LIST_DIR}/S5CGameplayEventTests.cpp
)
target_link_libraries(RenegadeS5CGameplayEventTests PRIVATE Renegade::EngineBridge)
target_compile_options(RenegadeS5CGameplayEventTests PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")
set_target_properties(RenegadeS5CGameplayEventTests PROPERTIES FOLDER "Renegade/Tests")
add_dependencies(RenegadeBridgeTests RenegadeS5CGameplayEventTests)
add_test(NAME RenegadeS5CGameplayEventTests COMMAND RenegadeS5CGameplayEventTests)
add_test(NAME RenegadeS5CGameplayEventSourceContract
    COMMAND ${CMAKE_COMMAND} -DRENEGADE_SOURCE_DIR=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_LIST_DIR}/S5CGameplayEventSourceContract.cmake)
