# A cross-volume build cannot derive shader source paths using a relative
# path from the build working directory. Adapt only the offline tool entry;
# rendering, runtime lookup and upstream files retain their accepted source.
set_source_files_properties(
    "${CMAKE_SOURCE_DIR}/WickedEngine/WickedEngine/offlineshadercompiler.cpp"
    TARGET_DIRECTORY offlineshadercompiler
    PROPERTIES COMPILE_DEFINITIONS "main=RenegadeOfflineShaderCompilerMain"
)
target_sources(offlineshadercompiler PRIVATE
    "${CMAKE_SOURCE_DIR}/Tools/OfflineShaderCompilerEntry.cpp"
)
target_compile_definitions(offlineshadercompiler PRIVATE
    RENEGADE_SHADER_SOURCE_DIRECTORY="${CMAKE_SOURCE_DIR}/WickedEngine/WickedEngine/shaders/"
)
