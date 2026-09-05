add_executable(RenegadeResourceImportTests
    ${CMAKE_CURRENT_LIST_DIR}/ResourceImportTests.cpp
)

target_link_libraries(
    RenegadeResourceImportTests
    PRIVATE
        Renegade::EngineBridge
)

target_compile_options(
    RenegadeResourceImportTests
    PRIVATE
        "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>"
)

set_target_properties(
    RenegadeResourceImportTests
    PROPERTIES
        FOLDER "Renegade/Tests"
)

# The Windows Studio workflow builds RenegadeBridgeTests explicitly before
# running CTest. Keep this Gate 1 executable in that targeted build graph.
add_dependencies(RenegadeBridgeTests RenegadeResourceImportTests)

add_test(
    NAME RenegadeResourceImportTests
    COMMAND RenegadeResourceImportTests
)
