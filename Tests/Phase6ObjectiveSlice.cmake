add_executable(RenegadePhase6ObjectiveSliceRuntimeTests
    ${CMAKE_CURRENT_LIST_DIR}/Phase6ObjectiveSliceRuntimeTests.cpp
)

target_link_libraries(
    RenegadePhase6ObjectiveSliceRuntimeTests
    PRIVATE
        Renegade::RuntimeBootstrap
)

target_compile_definitions(
    RenegadePhase6ObjectiveSliceRuntimeTests
    PRIVATE
        RENEGADE_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

target_compile_options(
    RenegadePhase6ObjectiveSliceRuntimeTests
    PRIVATE
        "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>"
)

set_target_properties(
    RenegadePhase6ObjectiveSliceRuntimeTests
    PROPERTIES
        FOLDER "Renegade/Tests"
)

add_dependencies(
    RenegadeBridgeTests
    RenegadePhase6ObjectiveSliceRuntimeTests
)

add_test(
    NAME RenegadePhase6ObjectiveSliceRuntimeTests
    COMMAND RenegadePhase6ObjectiveSliceRuntimeTests
)

set_tests_properties(
    RenegadePhase6ObjectiveSliceRuntimeTests
    PROPERTIES
        LABELS "Phase6;ObjectiveSlice"
)

# The native-navigation work is intentionally developed on a staging branch
# layered over this Phase 6 slice. Register its focused CPU-side Wicked
# VoxelGrid/PathQuery proof here without creating a second CI workflow.
include(${CMAKE_CURRENT_LIST_DIR}/Phase6NativeNavigation.cmake)
