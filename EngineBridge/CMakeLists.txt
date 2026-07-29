add_library(RenegadeEngineBridge STATIC
    include/renegade/bridge/SceneService.h
    include/renegade/bridge/SelectionService.h
    include/renegade/bridge/StudioSession.h
    src/SceneService.cpp
    src/SelectionService.cpp
)

add_library(Renegade::EngineBridge ALIAS RenegadeEngineBridge)

target_include_directories(RenegadeEngineBridge
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(RenegadeEngineBridge
    PUBLIC
        WickedEngine
)

set_target_properties(RenegadeEngineBridge PROPERTIES
    FOLDER "Renegade"
)
