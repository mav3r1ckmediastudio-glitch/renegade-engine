# LP07 Gate 5 — creator Asset Browser lifecycle and repeated RAsset placement.
#
# The representative FBX converter creates GPU-backed Wicked resources, so the
# lifecycle proof is built in both Debug and Release for compile/link coverage
# and executed Release-only on the hosted DX12 runner, matching Gates 1/3/4.
add_executable(RenegadeCreatorAssetWorkflowGraphicsProof
    ${CMAKE_CURRENT_LIST_DIR}/CreatorAssetWorkflowGraphicsProof.cpp
)

target_link_libraries(
    RenegadeCreatorAssetWorkflowGraphicsProof
    PRIVATE
        Renegade::EngineBridge
)

target_compile_definitions(
    RenegadeCreatorAssetWorkflowGraphicsProof
    PRIVATE UNICODE _UNICODE
)
target_compile_options(
    RenegadeCreatorAssetWorkflowGraphicsProof
    PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>"
)
set_target_properties(
    RenegadeCreatorAssetWorkflowGraphicsProof
    PROPERTIES FOLDER "Renegade/Tests"
)

# Studio CI explicitly builds RenegadeBridgeTests before invoking CTest. Keep
# the Gate 5 graphics proof in that chain in Debug and Release so a hidden API
# or link regression cannot escape simply because hosted execution is Release.
add_dependencies(
    RenegadeBridgeTests
    RenegadeCreatorAssetWorkflowGraphicsProof
)

add_custom_command(
    TARGET RenegadeCreatorAssetWorkflowGraphicsProof POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/WickedEngine/WickedEngine/dxcompiler.dll"
        "$<TARGET_FILE_DIR:RenegadeCreatorAssetWorkflowGraphicsProof>"
    VERBATIM
)

set(LP07_GATE5_STATIC_FBX_FIXTURE
    "${CMAKE_SOURCE_DIR}/Tests/Fixtures/LP07/maya_cube_6100_ascii.fbx"
)
set(LP07_GATE5_SKINNED_ANIMATED_FBX_FIXTURE
    "${CMAKE_SOURCE_DIR}/Tests/Fixtures/LP07/maya_transformed_skin_7700_ascii.fbx"
)

if(EXISTS "${LP07_GATE5_STATIC_FBX_FIXTURE}" AND
   EXISTS "${LP07_GATE5_SKINNED_ANIMATED_FBX_FIXTURE}")
    add_test(
        NAME RenegadeCreatorAssetWorkflowGraphicsProof
        COMMAND RenegadeCreatorAssetWorkflowGraphicsProof
            "${LP07_GATE5_STATIC_FBX_FIXTURE}"
            "${LP07_GATE5_SKINNED_ANIMATED_FBX_FIXTURE}"
            "${CMAKE_BINARY_DIR}/lp07-gate5-creator-asset-proof-output"
        CONFIGURATIONS Release
    )
    set_tests_properties(
        RenegadeCreatorAssetWorkflowGraphicsProof
        PROPERTIES
            TIMEOUT 300
            WORKING_DIRECTORY "$<TARGET_FILE_DIR:RenegadeCreatorAssetWorkflowGraphicsProof>"
    )
else()
    message(STATUS
        "LP07 Gate 5 graphics CTest not registered: immutable FBX fixtures are not staged."
    )
endif()
