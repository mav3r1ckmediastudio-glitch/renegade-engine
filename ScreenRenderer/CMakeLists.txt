add_library(RenegadeScreenRenderer STATIC
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
)

target_compile_options(RenegadeScreenRenderer
    PRIVATE
        "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>"
)

set_target_properties(RenegadeScreenRenderer PROPERTIES
    FOLDER "Renegade"
)
