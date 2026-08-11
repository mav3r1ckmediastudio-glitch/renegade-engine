# LP07 Gate 4 — stable explicit reimport.
#
# The real converter still creates GPU-backed Wicked resources, so the proof is
# built in Debug and Release but hosted execution remains Release-only, matching
# the accepted Gate 1/Gate 3 runner policy.
add_executable(RenegadeReusableAssetReimportGraphicsProof
    ${CMAKE_CURRENT_LIST_DIR}/ReusableAssetReimportGraphicsProof.cpp
)

target_link_libraries(
    RenegadeReusableAssetReimportGraphicsProof
    PRIVATE
        Renegade::EngineBridge
)

target_compile_definitions(
    RenegadeReusableAssetReimportGraphicsProof
    PRIVATE UNICODE _UNICODE
)
target_compile_options(
    RenegadeReusableAssetReimportGraphicsProof
    PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>"
)
set_target_properties(
    RenegadeReusableAssetReimportGraphicsProof
    PROPERTIES FOLDER "Renegade/Tests"
)

# Studio CI builds RenegadeBridgeTests explicitly before CTest. Keep the Gate 4
# proof in that build chain in both configurations even when Debug execution is
# excluded on the hosted DX12 runner.
add_dependencies(
    RenegadeBridgeTests
    RenegadeReusableAssetReimportGraphicsProof
)

add_custom_command(
    TARGET RenegadeReusableAssetReimportGraphicsProof POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/WickedEngine/WickedEngine/dxcompiler.dll"
        "$<TARGET_FILE_DIR:RenegadeReusableAssetReimportGraphicsProof>"
    VERBATIM
)

set(LP07_GATE4_STATIC_FBX_FIXTURE
    "${CMAKE_SOURCE_DIR}/Tests/Fixtures/LP07/maya_cube_6100_ascii.fbx"
)
set(LP07_GATE4_SKINNED_ANIMATED_FBX_FIXTURE
    "${CMAKE_SOURCE_DIR}/Tests/Fixtures/LP07/maya_transformed_skin_7700_ascii.fbx"
)

if(EXISTS "${LP07_GATE4_STATIC_FBX_FIXTURE}" AND
   EXISTS "${LP07_GATE4_SKINNED_ANIMATED_FBX_FIXTURE}")
    add_test(
        NAME RenegadeReusableAssetReimportGraphicsProof
        COMMAND RenegadeReusableAssetReimportGraphicsProof
            "${LP07_GATE4_STATIC_FBX_FIXTURE}"
            "${LP07_GATE4_SKINNED_ANIMATED_FBX_FIXTURE}"
            "${CMAKE_BINARY_DIR}/lp07-rasset-reimport-proof-output"
        CONFIGURATIONS Release
    )
    set_tests_properties(
        RenegadeReusableAssetReimportGraphicsProof
        PROPERTIES
            TIMEOUT 240
            WORKING_DIRECTORY "$<TARGET_FILE_DIR:RenegadeReusableAssetReimportGraphicsProof>"
    )
else()
    message(STATUS
        "LP07 Gate 4 graphics CTest not registered: immutable FBX fixtures are not staged."
    )
endif()
