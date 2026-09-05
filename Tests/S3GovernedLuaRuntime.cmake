add_executable(RenegadeS3GovernedLuaRuntimeTests
    ${CMAKE_CURRENT_LIST_DIR}/S3GovernedLuaRuntimeTests.cpp
)

target_link_libraries(
    RenegadeS3GovernedLuaRuntimeTests
    PRIVATE
        Renegade::RuntimeBootstrap
)

target_compile_options(
    RenegadeS3GovernedLuaRuntimeTests
    PRIVATE
        "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>"
)

set_target_properties(
    RenegadeS3GovernedLuaRuntimeTests
    PROPERTIES
        FOLDER "Renegade/Tests"
)

add_dependencies(
    RenegadeBridgeTests
    RenegadeS3GovernedLuaRuntimeTests
)

add_test(
    NAME RenegadeS3GovernedLuaRuntimeTests
    COMMAND RenegadeS3GovernedLuaRuntimeTests
)

add_test(
    NAME RenegadeS3GovernedLuaRuntimeSourceContract
    COMMAND ${CMAKE_COMMAND}
        -DRENEGADE_SOURCE_DIR=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_LIST_DIR}/S3GovernedLuaRuntimeSourceContract.cmake
)
