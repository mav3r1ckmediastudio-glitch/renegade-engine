add_executable(RenegadeStoryFlowGate4BLevelReferenceTests
    StoryFlowGate4BLevelReferenceTests.cpp
)

target_link_libraries(RenegadeStoryFlowGate4BLevelReferenceTests
    PRIVATE
        Renegade::EngineBridge
)

target_compile_definitions(RenegadeStoryFlowGate4BLevelReferenceTests
    PRIVATE
        UNICODE
        _UNICODE
)

target_compile_options(RenegadeStoryFlowGate4BLevelReferenceTests
    PRIVATE
        "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>"
)

set_target_properties(RenegadeStoryFlowGate4BLevelReferenceTests PROPERTIES
    FOLDER "Renegade/Tests"
)

add_test(
    NAME RenegadeStoryFlowGate4BLevelReferenceTests
    COMMAND RenegadeStoryFlowGate4BLevelReferenceTests
)
