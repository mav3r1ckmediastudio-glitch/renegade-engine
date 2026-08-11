add_executable(RenegadeReusableAssetTests
    ${CMAKE_CURRENT_LIST_DIR}/ReusableAssetTests.cpp
)

target_link_libraries(
    RenegadeReusableAssetTests
    PRIVATE
        Renegade::EngineBridge
)

set_target_properties(RenegadeReusableAssetTests PROPERTIES
    FOLDER "Renegade/Tests"
)

# Studio CI builds RenegadeBridgeTests explicitly before CTest. Keep the
# headless Gate 3 container/fail-closed contract in that build chain for both
# Debug and Release.
add_dependencies(RenegadeBridgeTests RenegadeReusableAssetTests)

add_test(NAME RenegadeReusableAssetTests COMMAND RenegadeReusableAssetTests)
