add_executable(RenegadeAssetCatalogueTests
    AssetCatalogueTests.cpp
)

target_link_libraries(
    RenegadeAssetCatalogueTests
    PRIVATE
        Renegade::EngineBridge
)

set_target_properties(RenegadeAssetCatalogueTests PROPERTIES
    FOLDER "Renegade/Tests"
)

# The Studio workflow builds RenegadeBridgeTests explicitly before running the
# complete CTest suite. Keep Gate 2 in that dependency chain so the registered
# executable is always present for Debug and Release CTest.
add_dependencies(RenegadeBridgeTests RenegadeAssetCatalogueTests)

add_test(NAME RenegadeAssetCatalogueTests COMMAND RenegadeAssetCatalogueTests)
