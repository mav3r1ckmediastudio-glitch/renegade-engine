# LP07 Gate 6 — persistent reusable asset identity across the real scene/build
# boundary. Fast headless proofs run in Debug and Release; the final creator ->
# LP05/LC01 -> LP06 -> named Runtime acceptance executes Release-only because
# representative FBX conversion and the real standalone Runtime require Wicked
# graphics initialization.
add_executable(RenegadeReusableAssetInstanceTests
    ${CMAKE_CURRENT_LIST_DIR}/ReusableAssetInstanceTests.cpp
)

target_link_libraries(
    RenegadeReusableAssetInstanceTests
    PRIVATE
        Renegade::EngineBridge
)

target_compile_definitions(
    RenegadeReusableAssetInstanceTests
    PRIVATE UNICODE _UNICODE
)
target_compile_options(
    RenegadeReusableAssetInstanceTests
    PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>"
)
set_target_properties(
    RenegadeReusableAssetInstanceTests
    PROPERTIES FOLDER "Renegade/Tests"
)

add_executable(RenegadeReusableAssetRuntimeTests
    ${CMAKE_CURRENT_LIST_DIR}/ReusableAssetRuntimeTests.cpp
)

target_link_libraries(
    RenegadeReusableAssetRuntimeTests
    PRIVATE
        Renegade::EngineBridge
)

target_compile_definitions(
    RenegadeReusableAssetRuntimeTests
    PRIVATE UNICODE _UNICODE
)
target_compile_options(
    RenegadeReusableAssetRuntimeTests
    PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>"
)
set_target_properties(
    RenegadeReusableAssetRuntimeTests
    PROPERTIES FOLDER "Renegade/Tests"
)

add_executable(RenegadeReusableAssetPackageAcceptance
    ${CMAKE_CURRENT_LIST_DIR}/ReusableAssetPackageAcceptance.cpp
)

target_link_libraries(
    RenegadeReusableAssetPackageAcceptance
    PRIVATE
        Renegade::EngineBridge
)

target_compile_definitions(
    RenegadeReusableAssetPackageAcceptance
    PRIVATE UNICODE _UNICODE
)
target_compile_options(
    RenegadeReusableAssetPackageAcceptance
    PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>"
)
set_target_properties(
    RenegadeReusableAssetPackageAcceptance
    PROPERTIES FOLDER "Renegade/Tests"
)
add_dependencies(
    RenegadeReusableAssetPackageAcceptance
    RenegadeRuntime
)

# Capture the exact checkout revisions into the acceptance invocation. LP06's
# staging contract deliberately rejects branch labels or synthetic version
# strings, and Wicked must remain at the pinned submodule commit.
find_package(Git REQUIRED)
execute_process(
    COMMAND "${GIT_EXECUTABLE}" rev-parse HEAD
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    RESULT_VARIABLE LP07_GATE6_RENEGADE_REVISION_RESULT
    OUTPUT_VARIABLE LP07_GATE6_RENEGADE_REVISION
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
execute_process(
    COMMAND "${GIT_EXECUTABLE}" rev-parse HEAD
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/WickedEngine"
    RESULT_VARIABLE LP07_GATE6_WICKED_REVISION_RESULT
    OUTPUT_VARIABLE LP07_GATE6_WICKED_REVISION
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
string(LENGTH "${LP07_GATE6_RENEGADE_REVISION}" LP07_GATE6_RENEGADE_REVISION_LENGTH)
string(LENGTH "${LP07_GATE6_WICKED_REVISION}" LP07_GATE6_WICKED_REVISION_LENGTH)
if(NOT LP07_GATE6_RENEGADE_REVISION_RESULT EQUAL 0 OR
   NOT LP07_GATE6_RENEGADE_REVISION_LENGTH EQUAL 40 OR
   NOT LP07_GATE6_WICKED_REVISION_RESULT EQUAL 0 OR
   NOT LP07_GATE6_WICKED_REVISION_LENGTH EQUAL 40)
    message(FATAL_ERROR "LP07 Gate 6 requires exact Renegade and Wicked revisions.")
endif()

add_dependencies(
    RenegadeBridgeTests
    RenegadeReusableAssetInstanceTests
    RenegadeReusableAssetRuntimeTests
    RenegadeReusableAssetPackageAcceptance
)

add_test(
    NAME RenegadeReusableAssetInstanceTests
    COMMAND RenegadeReusableAssetInstanceTests
        "${CMAKE_BINARY_DIR}/lp07-gate6-instance-proof-output"
)
set_tests_properties(
    RenegadeReusableAssetInstanceTests
    PROPERTIES TIMEOUT 60
)

add_test(
    NAME RenegadeReusableAssetRuntimeTests
    COMMAND RenegadeReusableAssetRuntimeTests
        "${CMAKE_BINARY_DIR}/lp07-gate6-runtime-refresh-output"
)
set_tests_properties(
    RenegadeReusableAssetRuntimeTests
    PROPERTIES TIMEOUT 60
)

set(LP07_GATE6_STATIC_FBX_FIXTURE
    "${CMAKE_SOURCE_DIR}/Tests/Fixtures/LP07/maya_cube_6100_ascii.fbx"
)
set(LP07_GATE6_ANIMATED_FBX_FIXTURE
    "${CMAKE_SOURCE_DIR}/Tests/Fixtures/LP07/maya_transformed_skin_7700_ascii.fbx"
)
set(LP07_GATE6_LP03_FIXTURE
    "${CMAKE_SOURCE_DIR}/Runtime/fixtures/LP03/Valid Screen"
)
set(LP07_GATE6_DERIVED_LP03_FIXTURE
    "${CMAKE_BINARY_DIR}/lp07-gate6-story-flow-fixture"
)
set(LP07_GATE6_PACKAGE_DOCS
    "${CMAKE_SOURCE_DIR}/Tests/fixtures/lp06_gate2"
)

if(EXISTS "${LP07_GATE6_STATIC_FBX_FIXTURE}" AND
   EXISTS "${LP07_GATE6_ANIMATED_FBX_FIXTURE}" AND
   EXISTS "${LP07_GATE6_LP03_FIXTURE}" AND
   EXISTS "${LP07_GATE6_PACKAGE_DOCS}")
    # The immutable LP03 fixture intentionally carries scene identity sidecars
    # but not the WISCENE payloads. Gate 6 creates LevelOne itself with the
    # reusable asset. Derive a disposable fixture and supply a separately
    # proven self-contained WISCENE as LevelTwo so the Story Flow is complete
    # before the stale-product freshness assertion is exercised.
    add_test(
        NAME RenegadeReusableAssetPackageFixture
        COMMAND ${CMAKE_COMMAND}
            "-DINPUT_FIXTURE=${LP07_GATE6_LP03_FIXTURE}"
            "-DLEVEL_TWO_SCENE=${RENEGADE_GATE4_SCENE}"
            "-DOUTPUT_FIXTURE=${LP07_GATE6_DERIVED_LP03_FIXTURE}"
            -P "${CMAKE_CURRENT_LIST_DIR}/PrepareLP07Gate6Fixture.cmake"
        CONFIGURATIONS Release
    )
    set_tests_properties(
        RenegadeReusableAssetPackageFixture
        PROPERTIES
            TIMEOUT 60
            FIXTURES_REQUIRED RenegadeGate4Scene
            FIXTURES_SETUP RenegadeLP07Gate6StoryFlow
    )

    add_test(
        NAME RenegadeReusableAssetPackageAcceptance
        COMMAND RenegadeReusableAssetPackageAcceptance
            "${LP07_GATE6_STATIC_FBX_FIXTURE}"
            "${LP07_GATE6_ANIMATED_FBX_FIXTURE}"
            "$<TARGET_FILE:RenegadeRuntime>"
            "$<TARGET_FILE_DIR:RenegadeRuntime>/dxcompiler.dll"
            "${LP07_GATE6_DERIVED_LP03_FIXTURE}"
            "${LP07_GATE6_PACKAGE_DOCS}"
            "${LP07_GATE6_RENEGADE_REVISION}"
            "${LP07_GATE6_WICKED_REVISION}"
        CONFIGURATIONS Release
    )
    set_tests_properties(
        RenegadeReusableAssetPackageAcceptance
        PROPERTIES
            TIMEOUT 900
            RUN_SERIAL TRUE
            FIXTURES_REQUIRED RenegadeLP07Gate6StoryFlow
            WORKING_DIRECTORY "$<TARGET_FILE_DIR:RenegadeReusableAssetPackageAcceptance>"
    )
else()
    message(STATUS
        "LP07 Gate 6 package acceptance not registered: required immutable fixtures are not staged."
    )
endif()
