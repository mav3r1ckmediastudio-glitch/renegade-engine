add_library(RenegadeScreenRenderer STATIC
    src/ScreenVideoAudio.cpp
    src/DialoguePanel.cpp
    include/renegade/screen/ScreenRenderer.h
    src/ScreenRenderer.cpp
)

add_library(Renegade::ScreenRenderer ALIAS RenegadeScreenRenderer)

target_include_directories(RenegadeScreenRenderer
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(RenegadeScreenRenderer
    PUBLIC
        Renegade::EngineBridge
    PRIVATE mfplat mfuuid shlwapi oleaut32 ole32
)

target_compile_options(RenegadeScreenRenderer
    PRIVATE
        "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>"
)

set_target_properties(RenegadeScreenRenderer PROPERTIES
    FOLDER "Renegade"
)
