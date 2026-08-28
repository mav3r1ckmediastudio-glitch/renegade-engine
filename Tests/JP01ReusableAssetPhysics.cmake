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

# The Studio CI builds RenegadeBridgeTests explicitly before CTest, so keep this
# owner-regression executable in that dependency chain.
add_dependencies(
    RenegadeBridgeTests
    RenegadeJP01ReusableAssetPhysicsTests
)

add_test(
    NAME RenegadeJP01ReusableAssetPhysicsTests
    COMMAND RenegadeJP01ReusableAssetPhysicsTests
)

# Cheap structural guard for the end-to-end owner hardening. The executable
# above proves behavior; this prevents a future refactor from quietly restoring
# the old per-frame scale tracker or anonymous-wrapper UI path.
add_test(
    NAME RenegadeJP01OwnerHardeningContract
    COMMAND ${CMAKE_COMMAND}
        -DRENEGADE_SOURCE_DIR=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_SOURCE_DIR}/Tests/JP01OwnerHardeningContract.cmake
)
