# LP07 Gate 1 real FBX conversion proof.
#
# The proof executable itself is always built on Windows. The CTest is only
# registered when the two immutable external fixtures have been staged before
# configure. GitHub Studio CI performs that staging and verifies each file by
# its upstream Git blob SHA before Build-Studio-Windows.ps1 configures CMake.
# Local developers can run the executable against any suitable FBX files
# directly without downloading the CI fixtures.

add_executable(RenegadeModelImportGraphicsProof
    "${CMAKE_SOURCE_DIR}/Tests/ModelImportGraphicsProof.cpp"
)

target_link_libraries(RenegadeModelImportGraphicsProof
    PRIVATE
        Renegade::EngineBridge
)

target_compile_definitions(RenegadeModelImportGraphicsProof
    PRIVATE
        UNICODE
        _UNICODE
)

target_compile_options(RenegadeModelImportGraphicsProof
    PRIVATE
        "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>"
)

set_target_properties(RenegadeModelImportGraphicsProof PROPERTIES
    FOLDER "Renegade/Tests"
)

# Studio CI builds RenegadeBridgeTests explicitly before running CTest.
add_dependencies(RenegadeBridgeTests RenegadeModelImportGraphicsProof)

add_custom_command(
    TARGET RenegadeModelImportGraphicsProof POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/WickedEngine/WickedEngine/dxcompiler.dll"
        "$<TARGET_FILE_DIR:RenegadeModelImportGraphicsProof>"
    VERBATIM
)

set(LP07_STATIC_FBX_FIXTURE
    "${CMAKE_SOURCE_DIR}/Tests/Fixtures/LP07/maya_cube_6100_ascii.fbx"
)
set(LP07_SKINNED_ANIMATED_FBX_FIXTURE
    "${CMAKE_SOURCE_DIR}/Tests/Fixtures/LP07/maya_transformed_skin_7700_ascii.fbx"
)

if(EXISTS "${LP07_STATIC_FBX_FIXTURE}" AND
   EXISTS "${LP07_SKINNED_ANIMATED_FBX_FIXTURE}")
    add_test(
        NAME RenegadeModelImportGraphicsProof
        COMMAND RenegadeModelImportGraphicsProof
            "${LP07_STATIC_FBX_FIXTURE}"
            "${LP07_SKINNED_ANIMATED_FBX_FIXTURE}"
            "${CMAKE_BINARY_DIR}/lp07-fbx-proof-output"
    )
    set_tests_properties(RenegadeModelImportGraphicsProof PROPERTIES
        TIMEOUT 180
        WORKING_DIRECTORY "$<TARGET_FILE_DIR:RenegadeModelImportGraphicsProof>"
    )
else()
    message(STATUS
        "LP07 FBX graphics CTest not registered: immutable proof fixtures are not staged."
    )
endif()
