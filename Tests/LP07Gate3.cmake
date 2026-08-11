add_executable(RenegadeReusableAssetTests
    ${CMAKE_CURRENT_LIST_DIR}/ReusableAssetTests.cpp
)

target_link_libraries(
    RenegadeReusableAssetTests
    PRIVATE
        Renegade::EngineBridge
)

set_target_properties(RenegadeReusableAssetTests PROPERTIES
    FOLDER "Renegade/Tests"
)

# Studio CI builds RenegadeBridgeTests explicitly before CTest. Keep the
# headless Gate 3 container/fail-closed contract in that build chain for both
# Debug and Release.
add_dependencies(RenegadeBridgeTests RenegadeReusableAssetTests)
add_test(NAME RenegadeReusableAssetTests COMMAND RenegadeReusableAssetTests)

# Real Gate 3 conversion/transaction proof. As in Gate 1, the executable is
# built in Debug and Release, while hosted execution is Release-only because
# pinned Wicked creates GPU-backed mesh resources during model conversion and
# its hosted Debug DX12 allocation path is not a supported runner capability.
add_executable(RenegadeReusableAssetGraphicsProof
    ${CMAKE_CURRENT_LIST_DIR}/ReusableAssetGraphicsProof.cpp
)

target_link_libraries(
    RenegadeReusableAssetGraphicsProof
    PRIVATE
        Renegade::EngineBridge
)

target_compile_definitions(RenegadeReusableAssetGraphicsProof PRIVATE UNICODE _UNICODE)
target_compile_options(RenegadeReusableAssetGraphicsProof
    PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>"
)
set_target_properties(RenegadeReusableAssetGraphicsProof PROPERTIES
    FOLDER "Renegade/Tests"
)
add_dependencies(RenegadeBridgeTests RenegadeReusableAssetGraphicsProof)

add_custom_command(
    TARGET RenegadeReusableAssetGraphicsProof POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/WickedEngine/WickedEngine/dxcompiler.dll"
        "$<TARGET_FILE_DIR:RenegadeReusableAssetGraphicsProof>"
    VERBATIM
)

set(LP07_GATE3_STATIC_FBX_FIXTURE
    "${CMAKE_SOURCE_DIR}/Tests/Fixtures/LP07/maya_cube_6100_ascii.fbx"
)
set(LP07_GATE3_SKINNED_ANIMATED_FBX_FIXTURE
    "${CMAKE_SOURCE_DIR}/Tests/Fixtures/LP07/maya_transformed_skin_7700_ascii.fbx"
)

if(EXISTS "${LP07_GATE3_STATIC_FBX_FIXTURE}" AND
   EXISTS "${LP07_GATE3_SKINNED_ANIMATED_FBX_FIXTURE}")
    add_test(
        NAME RenegadeReusableAssetGraphicsProof
        COMMAND RenegadeReusableAssetGraphicsProof
            "${LP07_GATE3_STATIC_FBX_FIXTURE}"
            "${LP07_GATE3_SKINNED_ANIMATED_FBX_FIXTURE}"
            "${CMAKE_BINARY_DIR}/lp07-rasset-proof-output"
        CONFIGURATIONS Release
    )
    set_tests_properties(RenegadeReusableAssetGraphicsProof PROPERTIES
        TIMEOUT 180
        WORKING_DIRECTORY "$<TARGET_FILE_DIR:RenegadeReusableAssetGraphicsProof>"
    )
else()
    message(STATUS
        "LP07 Gate 3 graphics CTest not registered: immutable FBX fixtures are not staged."
    )
endif()
