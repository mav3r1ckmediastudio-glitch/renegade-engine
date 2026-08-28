add_executable(RenegadeJP01ReusableAssetPhysicsTests
    ${CMAKE_CURRENT_LIST_DIR}/JP01ReusableAssetPhysicsTests.cpp
)

target_link_libraries(RenegadeJP01ReusableAssetPhysicsTests
    PRIVATE
        Renegade::EngineBridge
)

target_compile_options(RenegadeJP01ReusableAssetPhysicsTests
    PRIVATE
        "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>"
)

set_target_properties(RenegadeJP01ReusableAssetPhysicsTests PROPERTIES
    FOLDER "Renegade/Tests"
)

add_executable(RenegadeJP01TransformScaleCommitTests
    ${CMAKE_CURRENT_LIST_DIR}/JP01TransformScaleCommitTests.cpp
)

target_link_libraries(RenegadeJP01TransformScaleCommitTests
    PRIVATE
        Renegade::EngineBridge
)

target_compile_options(RenegadeJP01TransformScaleCommitTests
    PRIVATE
        "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>"
)

set_target_properties(RenegadeJP01TransformScaleCommitTests PROPERTIES
    FOLDER "Renegade/Tests"
)

# The Studio CI builds RenegadeBridgeTests explicitly before CTest, so keep both
# owner-regression executables in that dependency chain.
add_dependencies(
    RenegadeBridgeTests
    RenegadeJP01ReusableAssetPhysicsTests
    RenegadeJP01TransformScaleCommitTests
)

add_test(
    NAME RenegadeJP01ReusableAssetPhysicsTests
    COMMAND RenegadeJP01ReusableAssetPhysicsTests
)

add_test(
    NAME RenegadeJP01TransformScaleCommitTests
    COMMAND RenegadeJP01TransformScaleCommitTests
)
