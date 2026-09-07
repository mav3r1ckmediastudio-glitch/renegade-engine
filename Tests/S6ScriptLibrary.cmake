add_executable(RenegadeS6ScriptLibraryTests
    ${CMAKE_CURRENT_LIST_DIR}/S6ScriptLibraryTests.cpp
)

target_link_libraries(
    RenegadeS6ScriptLibraryTests
    PRIVATE
        Renegade::EngineBridge
)

target_compile_options(
    RenegadeS6ScriptLibraryTests
    PRIVATE
        "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>"
)

set_target_properties(
    RenegadeS6ScriptLibraryTests
    PROPERTIES
        FOLDER "Renegade/Tests"
)

add_dependencies(
    RenegadeBridgeTests
    RenegadeS6ScriptLibraryTests
)

add_test(
    NAME RenegadeS6ScriptLibraryTests
    COMMAND RenegadeS6ScriptLibraryTests
)

add_test(
    NAME RenegadeS6ScriptLibrarySourceContract
    COMMAND ${CMAKE_COMMAND}
        -DRENEGADE_SOURCE_DIR=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_LIST_DIR}/S6ScriptLibrarySourceContract.cmake
)
