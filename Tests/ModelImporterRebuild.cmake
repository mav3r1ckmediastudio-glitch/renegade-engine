add_executable(RenegadeModelImporterRebuildGraphicsProof
    ${CMAKE_CURRENT_LIST_DIR}/ModelImporterRebuildGraphicsProof.cpp
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
        "${CMAKE_SOURCE_DIR}/Tests/Fixtures/Importer/static_triangle.glb"
        "${CMAKE_SOURCE_DIR}/Tests/Fixtures/Importer/external_uri_triangle.glb"
        "${CMAKE_BINARY_DIR}/model-import-rebuild-proof"
    CONFIGURATIONS Release)
set_tests_properties(RenegadeModelImporterRebuildGraphicsProof PROPERTIES
    TIMEOUT 180
    WORKING_DIRECTORY "$<TARGET_FILE_DIR:RenegadeModelImporterRebuildGraphicsProof>")
