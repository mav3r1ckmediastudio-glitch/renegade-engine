add_library(RenegadeEngineBridge STATIC
    include/renegade/bridge/CommandService.h
    include/renegade/bridge/OceanService.h
    include/renegade/bridge/PrecipitationService.h
    include/renegade/bridge/SunService.h
    include/renegade/bridge/ProjectService.h
    include/renegade/bridge/SceneService.h
    include/renegade/bridge/SelectionService.h
    include/renegade/bridge/StudioSession.h
    src/CommandService.cpp
    src/OceanService.cpp
    src/PrecipitationService.cpp
    src/SunService.cpp
    src/ProjectService.cpp
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
