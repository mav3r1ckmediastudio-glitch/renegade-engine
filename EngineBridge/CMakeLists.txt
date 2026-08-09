add_library(RenegadeEngineBridge STATIC
    include/renegade/bridge/AssetBrowserService.h
    include/renegade/bridge/AssetRegistryService.h
    include/renegade/bridge/BuildService.h
    include/renegade/bridge/CommandService.h
    include/renegade/bridge/DependencyService.h
    include/renegade/bridge/FlowService.h
    include/renegade/bridge/ScreenService.h
    include/renegade/bridge/IdentityService.h
    include/renegade/bridge/LightService.h
    include/renegade/bridge/ImportService.h
    include/renegade/bridge/MaterialService.h
    include/renegade/bridge/CollisionService.h
    include/renegade/bridge/OceanService.h
    include/renegade/bridge/PrecipitationService.h
    include/renegade/bridge/SunService.h
    include/renegade/bridge/TerrainService.h
    include/renegade/bridge/ProjectService.h
    include/renegade/bridge/ProjectDocumentTransaction.h
    include/renegade/bridge/SceneService.h
    include/renegade/bridge/SceneDocumentService.h
    include/renegade/bridge/SelectionService.h
    include/renegade/bridge/StudioSession.h
    include/renegade/bridge/TestLevelSnapshotService.h
    src/AssetBrowserService.cpp
    src/AssetRegistryService.cpp
    src/BuildService.cpp
    src/CommandService.cpp
    src/DependencyService.cpp
    src/FlowService.cpp
    src/ScreenService.cpp
    src/IdentityService.cpp
    src/LightService.cpp
    src/ImportService.cpp
    src/MaterialService.cpp
    src/CollisionService.cpp
    src/OceanService.cpp
    src/PrecipitationService.cpp
    src/SunService.cpp
    src/TerrainService.cpp
    src/ProjectService.cpp
    src/ProjectDocumentTransaction.cpp
    src/SceneService.cpp
    src/SceneDocumentService.cpp
    src/SelectionService.cpp
    src/TestLevelSnapshotService.cpp
)

# Wicked's format converter is self-contained even though upstream keeps it in
# the reference Editor directory. Compile only that converter into Renegade's
# bridge; this does not enable or link the stock Wicked Editor application.
target_sources(RenegadeEngineBridge PRIVATE
    ${PROJECT_SOURCE_DIR}/WickedEngine/Editor/ModelImporter_GLTF.cpp
)

target_include_directories(RenegadeEngineBridge PRIVATE
    ${PROJECT_SOURCE_DIR}/WickedEngine/Editor
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
