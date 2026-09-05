# Story Flow Gate 2 — first-class Runtime Screen destinations.
# Kept separate from the historical Tests/CMakeLists.txt so the gate remains
# bounded and can be retired/folded later without disturbing older proofs.
add_executable(RenegadeStoryFlowScreenSemanticsTests
    ${CMAKE_CURRENT_LIST_DIR}/StoryFlowScreenSemanticsTests.cpp
)

target_link_libraries(
    RenegadeStoryFlowScreenSemanticsTests
    PRIVATE
        Renegade::RuntimeBootstrap
)

target_compile_options(
    RenegadeStoryFlowScreenSemanticsTests
    PRIVATE
        "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>"
)

set_target_properties(
    RenegadeStoryFlowScreenSemanticsTests
    PROPERTIES FOLDER "Renegade/Tests"
)

add_dependencies(
    RenegadeBridgeTests
    RenegadeStoryFlowScreenSemanticsTests
)

add_test(
    NAME RenegadeStoryFlowScreenSemanticsTests
    COMMAND RenegadeStoryFlowScreenSemanticsTests
)
