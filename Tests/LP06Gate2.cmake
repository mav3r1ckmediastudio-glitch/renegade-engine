add_executable(RenegadeBuildStageTests
    "${CMAKE_CURRENT_LIST_DIR}/BuildStageTests.cpp"
)
target_link_libraries(
    RenegadeBuildStageTests
    PRIVATE
        Renegade::EngineBridge
)
target_compile_options(
    RenegadeBuildStageTests
    PRIVATE
        "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>"
)
set_target_properties(RenegadeBuildStageTests PROPERTIES
    FOLDER "Renegade/Tests"
)
# Studio CI builds RenegadeBridgeTests explicitly before running CTest.
# Keep the Gate 2 staging proof in that targeted build chain.
add_dependencies(RenegadeBridgeTests RenegadeBuildStageTests)
add_test(
    NAME RenegadeBuildStageTests
    COMMAND RenegadeBuildStageTests
        "${CMAKE_CURRENT_LIST_DIR}/fixtures/lp06_gate2"
)
