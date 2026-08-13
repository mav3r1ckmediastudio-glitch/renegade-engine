# LP08 Gate 5 — governed resource dependency/package/Runtime acceptance.
# Headless stable-ID closure, live cache identity and package resolver proofs run
# Debug + Release. The final owner build and named Runtime proof is Release-only.
add_executable(RenegadeResourceAssetPackageRuntimeTests
    ${CMAKE_CURRENT_LIST_DIR}/ResourceAssetPackageRuntimeTests.cpp
)

target_link_libraries(
    RenegadeResourceAssetPackageRuntimeTests
    PRIVATE
        Renegade::EngineBridge
)

target_compile_options(
    RenegadeResourceAssetPackageRuntimeTests
    PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>"
)
set_target_properties(
    RenegadeResourceAssetPackageRuntimeTests
    PROPERTIES FOLDER "Renegade/Tests"
)

add_executable(RenegadeResourceAssetCacheIdentityTests
    ${CMAKE_CURRENT_LIST_DIR}/ResourceAssetCacheIdentityTests.cpp
)

target_link_libraries(
    RenegadeResourceAssetCacheIdentityTests
    PRIVATE
        Renegade::EngineBridge
)

target_compile_options(
    RenegadeResourceAssetCacheIdentityTests
    PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>"
)
set_target_properties(
    RenegadeResourceAssetCacheIdentityTests
    PROPERTIES FOLDER "Renegade/Tests"
)

add_executable(RenegadeResourceAssetPackageAcceptance
    ${CMAKE_CURRENT_LIST_DIR}/ResourceAssetPackageAcceptance.cpp
)

target_link_libraries(
    RenegadeResourceAssetPackageAcceptance
    PRIVATE
        Renegade::EngineBridge
)

target_compile_definitions(
    RenegadeResourceAssetPackageAcceptance
    PRIVATE UNICODE _UNICODE
)
target_compile_options(
    RenegadeResourceAssetPackageAcceptance
    PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>"
)
set_target_properties(
    RenegadeResourceAssetPackageAcceptance
    PROPERTIES FOLDER "Renegade/Tests"
)
add_dependencies(
    RenegadeResourceAssetPackageAcceptance
    RenegadeRuntime
)

find_package(Git REQUIRED)
execute_process(
    COMMAND "${GIT_EXECUTABLE}" rev-parse HEAD
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    RESULT_VARIABLE LP08_GATE5_RENEGADE_REVISION_RESULT
    OUTPUT_VARIABLE LP08_GATE5_RENEGADE_REVISION
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
execute_process(
    COMMAND "${GIT_EXECUTABLE}" rev-parse HEAD
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/WickedEngine"
    RESULT_VARIABLE LP08_GATE5_WICKED_REVISION_RESULT
    OUTPUT_VARIABLE LP08_GATE5_WICKED_REVISION
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
string(LENGTH "${LP08_GATE5_RENEGADE_REVISION}" LP08_GATE5_RENEGADE_REVISION_LENGTH)
string(LENGTH "${LP08_GATE5_WICKED_REVISION}" LP08_GATE5_WICKED_REVISION_LENGTH)
if(NOT LP08_GATE5_RENEGADE_REVISION_RESULT EQUAL 0 OR
   NOT LP08_GATE5_RENEGADE_REVISION_LENGTH EQUAL 40 OR
   NOT LP08_GATE5_WICKED_REVISION_RESULT EQUAL 0 OR
   NOT LP08_GATE5_WICKED_REVISION_LENGTH EQUAL 40)
    message(FATAL_ERROR "LP08 Gate 5 requires exact Renegade and Wicked revisions.")
endif()

add_dependencies(
    RenegadeBridgeTests
    RenegadeResourceAssetPackageRuntimeTests
    RenegadeResourceAssetCacheIdentityTests
    RenegadeResourceAssetPackageAcceptance
)

add_test(
    NAME RenegadeResourceAssetPackageRuntimeTests
    COMMAND RenegadeResourceAssetPackageRuntimeTests
        "${CMAKE_CURRENT_BINARY_DIR}/lp08-gate5-package-runtime"
)
set_tests_properties(
    RenegadeResourceAssetPackageRuntimeTests
    PROPERTIES TIMEOUT 120
)

add_test(
    NAME RenegadeResourceAssetCacheIdentityTests
    COMMAND RenegadeResourceAssetCacheIdentityTests
)
set_tests_properties(
    RenegadeResourceAssetCacheIdentityTests
    PROPERTIES TIMEOUT 60
)

set(LP08_GATE5_LP03_FIXTURE
    "${CMAKE_SOURCE_DIR}/Runtime/fixtures/LP03/Valid Screen"
)
set(LP08_GATE5_DERIVED_LP03_FIXTURE
    "${CMAKE_BINARY_DIR}/lp08-gate5-story-flow-fixture"
)
set(LP08_GATE5_PACKAGE_DOCS
    "${CMAKE_SOURCE_DIR}/Tests/fixtures/lp06_gate2"
)

if(EXISTS "${LP08_GATE5_LP03_FIXTURE}" AND
   EXISTS "${LP08_GATE5_PACKAGE_DOCS}")
    add_test(
        NAME RenegadeResourceAssetPackageFixture
        COMMAND ${CMAKE_COMMAND}
            "-DINPUT_FIXTURE=${LP08_GATE5_LP03_FIXTURE}"
            "-DLEVEL_TWO_SCENE=${RENEGADE_GATE4_SCENE}"
            "-DOUTPUT_FIXTURE=${LP08_GATE5_DERIVED_LP03_FIXTURE}"
            -P "${CMAKE_CURRENT_LIST_DIR}/PrepareLP07Gate6Fixture.cmake"
        CONFIGURATIONS Release
    )
    set_tests_properties(
        RenegadeResourceAssetPackageFixture
        PROPERTIES
            TIMEOUT 60
            FIXTURES_REQUIRED RenegadeGate4Scene
            FIXTURES_SETUP RenegadeLP08Gate5StoryFlow
    )

    add_test(
        NAME RenegadeResourceAssetPackageAcceptance
        COMMAND RenegadeResourceAssetPackageAcceptance
            "$<TARGET_FILE:RenegadeRuntime>"
            "$<TARGET_FILE_DIR:RenegadeRuntime>/dxcompiler.dll"
            "${LP08_GATE5_DERIVED_LP03_FIXTURE}"
            "${LP08_GATE5_PACKAGE_DOCS}"
            "${LP08_GATE5_RENEGADE_REVISION}"
            "${LP08_GATE5_WICKED_REVISION}"
        CONFIGURATIONS Release
    )
    set_tests_properties(
        RenegadeResourceAssetPackageAcceptance
        PROPERTIES
            TIMEOUT 900
            RUN_SERIAL TRUE
            FIXTURES_REQUIRED RenegadeLP08Gate5StoryFlow
            WORKING_DIRECTORY "$<TARGET_FILE_DIR:RenegadeResourceAssetPackageAcceptance>"
    )
else()
    message(FATAL_ERROR
        "LP08 Gate 5 package acceptance requires the immutable LP03/package-document fixtures; refusing to configure without proof inputs."
    )
endif()
