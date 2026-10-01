add_executable(RenegadeModelImporterRebuildGraphicsProof
    ${CMAKE_CURRENT_LIST_DIR}/ModelImporterRebuildGraphicsProof.cpp
    ${CMAKE_SOURCE_DIR}/Studio/src/ModelImportPreview.cpp
)
target_link_libraries(RenegadeModelImporterRebuildGraphicsProof
    PRIVATE Renegade::EngineBridge)
target_compile_definitions(RenegadeModelImporterRebuildGraphicsProof
    PRIVATE UNICODE _UNICODE)
target_compile_options(RenegadeModelImporterRebuildGraphicsProof
    PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")
set_target_properties(RenegadeModelImporterRebuildGraphicsProof
    PROPERTIES FOLDER "Renegade/Tests")
add_custom_command(TARGET RenegadeModelImporterRebuildGraphicsProof POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/WickedEngine/WickedEngine/dxcompiler.dll"
        "$<TARGET_FILE_DIR:RenegadeModelImporterRebuildGraphicsProof>"
    VERBATIM)
add_test(NAME RenegadeModelImporterRebuildGraphicsProof
    COMMAND RenegadeModelImporterRebuildGraphicsProof
        "${CMAKE_SOURCE_DIR}/Tests/fixtures/Importer/static_triangle.glb"
        "${CMAKE_SOURCE_DIR}/Tests/fixtures/Importer/external_uri_triangle.glb"
        "${CMAKE_BINARY_DIR}/model-import-rebuild-proof"
    CONFIGURATIONS Release)
set_tests_properties(RenegadeModelImporterRebuildGraphicsProof PROPERTIES
    TIMEOUT 180
    WORKING_DIRECTORY "$<TARGET_FILE_DIR:RenegadeModelImporterRebuildGraphicsProof>")

foreach(fbx_fixture IN ITEMS static_textured_cube static_embedded_cube animated_character)
    add_test(NAME RenegadeModelImporterRebuild_${fbx_fixture}
        COMMAND RenegadeModelImporterRebuildGraphicsProof
            "${CMAKE_SOURCE_DIR}/Tests/fixtures/Importer/${fbx_fixture}.fbx"
            "${CMAKE_SOURCE_DIR}/Tests/fixtures/Importer/external_uri_triangle.glb"
            "${CMAKE_BINARY_DIR}/model-import-${fbx_fixture}-proof"
        CONFIGURATIONS Release)
    set_tests_properties(RenegadeModelImporterRebuild_${fbx_fixture} PROPERTIES
        FIXTURES_SETUP ${fbx_fixture}
        TIMEOUT 180 WORKING_DIRECTORY "$<TARGET_FILE_DIR:RenegadeModelImporterRebuildGraphicsProof>")
    add_test(NAME RenegadeModelImporterRebuild_${fbx_fixture}_ColdReopen
        COMMAND RenegadeModelImporterRebuildGraphicsProof --reopen
            "${CMAKE_BINARY_DIR}/model-import-${fbx_fixture}-proof"
            "SourceAssets/Models/Proof Triangle/${fbx_fixture}.fbx"
        CONFIGURATIONS Release)
    set_tests_properties(RenegadeModelImporterRebuild_${fbx_fixture}_ColdReopen PROPERTIES
        DEPENDS RenegadeModelImporterRebuild_${fbx_fixture}
        FIXTURES_REQUIRED ${fbx_fixture}
        TIMEOUT 180 WORKING_DIRECTORY "$<TARGET_FILE_DIR:RenegadeModelImporterRebuildGraphicsProof>")
endforeach()
