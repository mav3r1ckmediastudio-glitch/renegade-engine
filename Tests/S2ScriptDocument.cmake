add_executable(RenegadeS2ScriptDocumentTests
    ${CMAKE_CURRENT_LIST_DIR}/S2ScriptDocumentTests.cpp
)

target_link_libraries(
    RenegadeS2ScriptDocumentTests
    PRIVATE
        Renegade::EngineBridge
)

target_compile_options(
    RenegadeS2ScriptDocumentTests
    PRIVATE
        "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>"
)

set_target_properties(
    RenegadeS2ScriptDocumentTests
    PROPERTIES
        FOLDER "Renegade/Tests"
)

add_dependencies(RenegadeBridgeTests RenegadeS2ScriptDocumentTests)
add_test(
    NAME RenegadeS2ScriptDocumentTests
    COMMAND RenegadeS2ScriptDocumentTests
)

add_test(
    NAME RenegadeS2ScriptDocumentSourceContract
    COMMAND ${CMAKE_COMMAND}
        -DRENEGADE_SOURCE_DIR=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_LIST_DIR}/S2ScriptDocumentSourceContract.cmake
)
