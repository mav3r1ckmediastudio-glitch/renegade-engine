add_executable(RenegadeS1AInspectorSectionFrameworkTests
    "${CMAKE_CURRENT_LIST_DIR}/S1AInspectorSectionFrameworkTests.cpp"
)

target_link_libraries(RenegadeS1AInspectorSectionFrameworkTests
    PRIVATE
        Renegade::InspectorSectionFramework
)

set_target_properties(RenegadeS1AInspectorSectionFrameworkTests PROPERTIES
    FOLDER "Renegade/Tests"
)

# Studio CI builds RenegadeBridgeTests explicitly before running CTest. Keep
# the lightweight S1A framework proof in that targeted build graph.
add_dependencies(RenegadeBridgeTests RenegadeS1AInspectorSectionFrameworkTests)

add_test(
    NAME RenegadeS1AInspectorSectionFrameworkTests
    COMMAND RenegadeS1AInspectorSectionFrameworkTests
)

add_test(
    NAME RenegadeS1AInspectorSectionSourceContract
    COMMAND ${CMAKE_COMMAND}
        -DRENEGADE_SOURCE_DIR=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_LIST_DIR}/S1AInspectorSectionSourceContract.cmake
)
