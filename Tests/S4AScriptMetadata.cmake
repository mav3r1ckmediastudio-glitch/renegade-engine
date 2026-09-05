add_executable(RenegadeS4AScriptMetadataTests
    ${CMAKE_CURRENT_LIST_DIR}/S4AScriptMetadataTests.cpp
)

target_link_libraries(
    RenegadeS4AScriptMetadataTests
    PRIVATE
        Renegade::EngineBridge
)

target_compile_options(
    RenegadeS4AScriptMetadataTests
    PRIVATE
        "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>"
)

set_target_properties(
    RenegadeS4AScriptMetadataTests
    PROPERTIES
        FOLDER "Renegade/Tests"
)

add_dependencies(
    RenegadeBridgeTests
    RenegadeS4AScriptMetadataTests
)

add_test(
    NAME RenegadeS4AScriptMetadataTests
    COMMAND RenegadeS4AScriptMetadataTests
)

add_test(
    NAME RenegadeS4AScriptMetadataSourceContract
    COMMAND ${CMAKE_COMMAND}
        -DRENEGADE_SOURCE_DIR=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_LIST_DIR}/S4AScriptMetadataSourceContract.cmake
)
