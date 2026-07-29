add_executable(RenegadeStudio WIN32
    "${CMAKE_SOURCE_DIR}/WickedEngine/Editor/Translator.cpp"
    src/StudioApplication.cpp
    src/StudioApplication.h
    src/main_Windows.cpp
)

target_include_directories(RenegadeStudio
    PRIVATE
        "${CMAKE_SOURCE_DIR}/WickedEngine/Editor"
)

target_link_libraries(RenegadeStudio
    PRIVATE
        Renegade::EngineBridge
)

target_compile_definitions(RenegadeStudio
    PRIVATE
        UNICODE
        _UNICODE
)

set_target_properties(RenegadeStudio PROPERTIES
    FOLDER "Renegade"
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:RenegadeStudio>"
)

add_custom_command(
    TARGET RenegadeStudio POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:RenegadeStudio>/Content"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/WickedEngine/WickedEngine/dxcompiler.dll"
        "$<TARGET_FILE_DIR:RenegadeStudio>"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/WickedEngine/Content/models/cube.wiscene"
        "$<TARGET_FILE_DIR:RenegadeStudio>/Content/cube.wiscene"
    VERBATIM
)
