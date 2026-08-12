# LP08 Gate 3 — governed texture creator import and base-colour material binding.
# Headless stable-ID/Undo/Redo/Save/Open proof runs in both configurations.
add_executable(RenegadeMaterialTextureAssetTests
    ${CMAKE_CURRENT_LIST_DIR}/MaterialTextureAssetTests.cpp
)

target_link_libraries(
    RenegadeMaterialTextureAssetTests
    PRIVATE
        Renegade::EngineBridge
)

target_compile_options(
    RenegadeMaterialTextureAssetTests
    PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>"
)
set_target_properties(
    RenegadeMaterialTextureAssetTests
    PROPERTIES FOLDER "Renegade/Tests"
)

# Real wi::resourcemanager PNG decoding creates a GPU-backed Wicked texture.
# Compile/link in Debug and Release, execute Release-only on hosted DX12 just
# like the accepted LP07 importer graphics proofs.
add_executable(RenegadeMaterialTextureGraphicsProof
    ${CMAKE_CURRENT_LIST_DIR}/MaterialTextureGraphicsProof.cpp
)

target_link_libraries(
    RenegadeMaterialTextureGraphicsProof
    PRIVATE
        Renegade::EngineBridge
)

target_compile_definitions(
    RenegadeMaterialTextureGraphicsProof
    PRIVATE UNICODE _UNICODE
)
target_compile_options(
    RenegadeMaterialTextureGraphicsProof
    PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>"
)
set_target_properties(
    RenegadeMaterialTextureGraphicsProof
    PROPERTIES FOLDER "Renegade/Tests"
)

add_dependencies(
    RenegadeBridgeTests
    RenegadeMaterialTextureAssetTests
    RenegadeMaterialTextureGraphicsProof
)

add_custom_command(
    TARGET RenegadeMaterialTextureGraphicsProof POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/WickedEngine/WickedEngine/dxcompiler.dll"
        "$<TARGET_FILE_DIR:RenegadeMaterialTextureGraphicsProof>"
    VERBATIM
)

add_test(
    NAME RenegadeMaterialTextureAssetTests
    COMMAND RenegadeMaterialTextureAssetTests
)
set_tests_properties(
    RenegadeMaterialTextureAssetTests
    PROPERTIES TIMEOUT 60
)

add_test(
    NAME RenegadeMaterialTextureGraphicsProof
    COMMAND RenegadeMaterialTextureGraphicsProof
        "${CMAKE_BINARY_DIR}/lp08-gate3-material-texture-proof-output"
    CONFIGURATIONS Release
)
set_tests_properties(
    RenegadeMaterialTextureGraphicsProof
    PROPERTIES
        TIMEOUT 120
        WORKING_DIRECTORY "$<TARGET_FILE_DIR:RenegadeMaterialTextureGraphicsProof>"
)
