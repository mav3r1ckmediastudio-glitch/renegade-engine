add_executable(RenegadeBuildPlanTests
    "${CMAKE_CURRENT_LIST_DIR}/BuildPlanTests.cpp"
)
target_link_libraries(
    RenegadeBuildPlanTests
    PRIVATE
        Renegade::EngineBridge
)
target_compile_options(
    RenegadeBuildPlanTests
    PRIVATE
        "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>"
)
set_target_properties(RenegadeBuildPlanTests PROPERTIES
    FOLDER "Renegade/Tests"
)
# The Studio CI builds RenegadeBridgeTests explicitly before running CTest.
# Keep the LP06 Gate 1 executable in that targeted build chain.
add_dependencies(RenegadeBridgeTests RenegadeBuildPlanTests)
add_test(NAME RenegadeBuildPlanTests COMMAND RenegadeBuildPlanTests)
