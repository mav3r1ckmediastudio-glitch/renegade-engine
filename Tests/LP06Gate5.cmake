add_executable(RenegadeBuildPromotionTests
    "${CMAKE_CURRENT_LIST_DIR}/BuildPromotionTests.cpp"
)
target_link_libraries(
    RenegadeBuildPromotionTests
    PRIVATE
        Renegade::EngineBridge
)
target_include_directories(
    RenegadeBuildPromotionTests
    PRIVATE
        "${PROJECT_SOURCE_DIR}/EngineBridge/src"
        # json.hpp is the pinned nlohmann single-header copy shipped with the
        # Wicked Editor sources. EngineBridge already consumes the same pinned
        # header privately; expose that exact include directory to this test
        # target rather than adding another JSON dependency or copying it.
        "${PROJECT_SOURCE_DIR}/WickedEngine/Editor"
)
target_compile_definitions(
    RenegadeBuildPromotionTests
    PRIVATE
        UNICODE
        _UNICODE
)
target_compile_options(
    RenegadeBuildPromotionTests
    PRIVATE
        "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>"
)
set_target_properties(RenegadeBuildPromotionTests PROPERTIES
    FOLDER "Renegade/Tests"
)

add_executable(RenegadeWindowsGameBuildProjectTests
    "${CMAKE_CURRENT_LIST_DIR}/WindowsGameBuildProjectTests.cpp"
)
target_link_libraries(
    RenegadeWindowsGameBuildProjectTests
    PRIVATE
        Renegade::EngineBridge
)
target_compile_definitions(
    RenegadeWindowsGameBuildProjectTests
    PRIVATE
        UNICODE
        _UNICODE
)
target_compile_options(
    RenegadeWindowsGameBuildProjectTests
    PRIVATE
        "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>"
)
set_target_properties(RenegadeWindowsGameBuildProjectTests PROPERTIES
    FOLDER "Renegade/Tests"
)

# Studio CI explicitly builds RenegadeBridgeTests before CTest. Keep the Gate 5
# transaction proof and owner-build regression in that authoritative targeted
# build chain.
add_dependencies(
    RenegadeBridgeTests
    RenegadeBuildPromotionTests
    RenegadeWindowsGameBuildProjectTests
)

set(RENEGADE_GATE5_CASES
    first
    replace
    incomplete
    failed-smoke
    locked
    interrupted
    post-move-validation
    stale-recovery
)

foreach(RENEGADE_GATE5_CASE IN LISTS RENEGADE_GATE5_CASES)
    string(REPLACE "-" "_" RENEGADE_GATE5_TEST_SUFFIX
        "${RENEGADE_GATE5_CASE}")
    add_test(
        NAME "RenegadeBuildPromotion_${RENEGADE_GATE5_TEST_SUFFIX}"
        COMMAND RenegadeBuildPromotionTests "${RENEGADE_GATE5_CASE}"
    )
    set_tests_properties(
        "RenegadeBuildPromotion_${RENEGADE_GATE5_TEST_SUFFIX}"
        PROPERTIES
            TIMEOUT 30
            RUN_SERIAL TRUE
    )
endforeach()

add_test(
    NAME RenegadeWindowsGameBuildProjectTests
    COMMAND RenegadeWindowsGameBuildProjectTests
        "${PROJECT_SOURCE_DIR}/Runtime/fixtures/LP03/Valid Screen"
        "${PROJECT_SOURCE_DIR}/WickedEngine/Content/models/cube.wiscene"
)
set_tests_properties(
    RenegadeWindowsGameBuildProjectTests
    PROPERTIES
        TIMEOUT 120
        RUN_SERIAL TRUE
)
