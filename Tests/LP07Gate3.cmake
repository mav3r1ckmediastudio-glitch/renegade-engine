# LP07 Gate 3 coverage retained after the clean importer rebuild.
# Import/conversion transaction proofs moved to ModelImporterRebuild.cmake.
# This gate now keeps the reusable-product document contract and the real
# Wicked importer failure-adapter regression.

add_executable(RenegadeReusableAssetTests
    ${CMAKE_CURRENT_LIST_DIR}/ReusableAssetTests.cpp
)
target_link_libraries(RenegadeReusableAssetTests
    PRIVATE Renegade::EngineBridge
)
set_target_properties(RenegadeReusableAssetTests PROPERTIES
    FOLDER "Renegade/Tests"
)
add_dependencies(RenegadeBridgeTests RenegadeReusableAssetTests)
add_test(NAME RenegadeReusableAssetTests COMMAND RenegadeReusableAssetTests)

add_executable(RenegadeModelImporterFailureAdapterTests
    ${CMAKE_CURRENT_LIST_DIR}/ModelImporterFailureAdapterTests.cpp
)
target_link_libraries(RenegadeModelImporterFailureAdapterTests
    PRIVATE Renegade::EngineBridge
)
target_include_directories(RenegadeModelImporterFailureAdapterTests PRIVATE
    "${CMAKE_SOURCE_DIR}/WickedEngine/Editor"
)
set_target_properties(RenegadeModelImporterFailureAdapterTests PROPERTIES
    FOLDER "Renegade/Tests"
)
add_dependencies(
    RenegadeBridgeTests
    RenegadeModelImporterFailureAdapterTests
)
add_test(
    NAME RenegadeModelImporterFailureAdapterTests
    COMMAND RenegadeModelImporterFailureAdapterTests
        "${CMAKE_BINARY_DIR}/lp07-import-failure-adapter"
)
set_tests_properties(
    RenegadeModelImporterFailureAdapterTests
    PROPERTIES TIMEOUT 30
)

message(STATUS
    "LP07 Gate 3 legacy import transactions retired; current importer proof is ModelImporterRebuild."
)
