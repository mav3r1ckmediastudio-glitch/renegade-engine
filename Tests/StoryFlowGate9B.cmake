# Story Flow Gate 9B — native Journey cards, lanes and governed Level thumbnails.
add_executable(RenegadeStoryFlowGate9BJourneyThumbnailTests
    ${CMAKE_CURRENT_LIST_DIR}/StoryFlowGate9BJourneyThumbnailTests.cpp
)

target_link_libraries(
    RenegadeStoryFlowGate9BJourneyThumbnailTests
    PRIVATE
        Renegade::EngineBridge
)

target_compile_options(
    RenegadeStoryFlowGate9BJourneyThumbnailTests
    PRIVATE
        "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>"
)

set_target_properties(
    RenegadeStoryFlowGate9BJourneyThumbnailTests
    PROPERTIES FOLDER "Renegade/Tests"
)

add_dependencies(
    RenegadeBridgeTests
    RenegadeStoryFlowGate9BJourneyThumbnailTests
)

add_test(
    NAME RenegadeStoryFlowGate9BJourneyThumbnailTests
    COMMAND RenegadeStoryFlowGate9BJourneyThumbnailTests
)
