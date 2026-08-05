add_executable(RenegadeBridgeTests
    BridgeCommandTests.cpp
)

target_link_libraries(RenegadeBridgeTests
    PRIVATE
        Renegade::EngineBridge
)

set_target_properties(RenegadeBridgeTests PROPERTIES
    FOLDER "Renegade"
)

add_test(NAME RenegadeBridgeTests COMMAND RenegadeBridgeTests)

add_executable(RenegadePrecipitationTests
    PrecipitationTests.cpp
)

target_link_libraries(RenegadePrecipitationTests
    PRIVATE
        Renegade::EngineBridge
)

set_target_properties(RenegadePrecipitationTests PROPERTIES
    FOLDER "Renegade"
)

# The Windows Studio CI builds RenegadeBridgeTests explicitly before running
# the complete CTest suite. Keep the precipitation executable in that build
# chain so CTest never registers a test binary that the targeted build omitted.
add_dependencies(RenegadeBridgeTests RenegadePrecipitationTests)

add_test(NAME RenegadePrecipitationTests COMMAND RenegadePrecipitationTests)

add_executable(RenegadeSunTests
    SunTests.cpp
)

target_link_libraries(RenegadeSunTests
    PRIVATE
        Renegade::EngineBridge
)

set_target_properties(RenegadeSunTests PROPERTIES
    FOLDER "Renegade"
)

# Windows Studio CI builds the bridge target explicitly before CTest.
add_dependencies(RenegadeBridgeTests RenegadeSunTests)

add_test(NAME RenegadeSunTests COMMAND RenegadeSunTests)

add_executable(RenegadeOceanTests
    OceanTests.cpp
)

target_link_libraries(RenegadeOceanTests
    PRIVATE
        Renegade::EngineBridge
)

set_target_properties(RenegadeOceanTests PROPERTIES
    FOLDER "Renegade"
)

# Windows Studio CI builds the bridge target explicitly before CTest.
add_dependencies(RenegadeBridgeTests RenegadeOceanTests)

add_test(NAME RenegadeOceanTests COMMAND RenegadeOceanTests)

add_executable(RenegadeTerrainTests
    TerrainTests.cpp
)

target_link_libraries(RenegadeTerrainTests
    PRIVATE
        Renegade::EngineBridge
)

set_target_properties(RenegadeTerrainTests PROPERTIES
    FOLDER "Renegade"
)

add_dependencies(RenegadeBridgeTests RenegadeTerrainTests)

add_test(NAME RenegadeTerrainTests COMMAND RenegadeTerrainTests)

add_executable(RenegadeLightTests
    LightTests.cpp
)

target_link_libraries(RenegadeLightTests
    PRIVATE
        Renegade::EngineBridge
)

set_target_properties(RenegadeLightTests PROPERTIES
    FOLDER "Renegade"
)

add_dependencies(RenegadeBridgeTests RenegadeLightTests)

add_test(NAME RenegadeLightTests COMMAND RenegadeLightTests)

add_executable(RenegadeMaterialTests
    MaterialTests.cpp
)

target_link_libraries(RenegadeMaterialTests
    PRIVATE
        Renegade::EngineBridge
)

set_target_properties(RenegadeMaterialTests PROPERTIES
    FOLDER "Renegade"
)

add_dependencies(RenegadeBridgeTests RenegadeMaterialTests)

add_test(NAME RenegadeMaterialTests COMMAND RenegadeMaterialTests)

add_executable(RenegadeImportTests
    ImportTests.cpp
)

target_link_libraries(RenegadeImportTests
    PRIVATE
        Renegade::EngineBridge
)

set_target_properties(RenegadeImportTests PROPERTIES
    FOLDER "Renegade"
)

add_dependencies(RenegadeBridgeTests RenegadeImportTests)

add_test(NAME RenegadeImportTests COMMAND RenegadeImportTests)

add_executable(RenegadeCollisionTests
    CollisionTests.cpp
)

target_link_libraries(RenegadeCollisionTests
    PRIVATE
        Renegade::EngineBridge
)

set_target_properties(RenegadeCollisionTests PROPERTIES
    FOLDER "Renegade"
)

add_dependencies(RenegadeBridgeTests RenegadeCollisionTests)

add_test(NAME RenegadeCollisionTests COMMAND RenegadeCollisionTests)

add_executable(RenegadeAssetBrowserTests
    AssetBrowserTests.cpp
)

target_link_libraries(RenegadeAssetBrowserTests
    PRIVATE
        Renegade::EngineBridge
)

set_target_properties(RenegadeAssetBrowserTests PROPERTIES
    FOLDER "Renegade"
)

add_dependencies(RenegadeBridgeTests RenegadeAssetBrowserTests)

add_test(NAME RenegadeAssetBrowserTests COMMAND RenegadeAssetBrowserTests)

add_executable(RenegadeProjectStructureTests
    ProjectStructureTests.cpp
)

target_link_libraries(RenegadeProjectStructureTests
    PRIVATE
        Renegade::EngineBridge
)

set_target_properties(RenegadeProjectStructureTests PROPERTIES
    FOLDER "Renegade"
)

add_dependencies(RenegadeBridgeTests RenegadeProjectStructureTests)

add_test(
    NAME RenegadeProjectStructureTests
    COMMAND RenegadeProjectStructureTests
)


add_executable(RenegadeRuntimeBootstrapTests
    RuntimeBootstrapTests.cpp
)

target_link_libraries(RenegadeRuntimeBootstrapTests
    PRIVATE
        Renegade::RuntimeBootstrap
)

target_compile_options(RenegadeRuntimeBootstrapTests
    PRIVATE
        "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>"
)

set_target_properties(RenegadeRuntimeBootstrapTests PROPERTIES
    FOLDER "Renegade"
)

add_dependencies(RenegadeBridgeTests RenegadeRuntimeBootstrapTests)

add_test(
    NAME RenegadeRuntimeBootstrapTests
    COMMAND RenegadeRuntimeBootstrapTests
)
