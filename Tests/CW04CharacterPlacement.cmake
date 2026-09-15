add_executable(RenegadeCW04CharacterPlacementTests
    ${CMAKE_CURRENT_LIST_DIR}/CW04CharacterPlacementTests.cpp
)

target_link_libraries(
    RenegadeCW04CharacterPlacementTests
    PRIVATE
        Renegade::EngineBridge
)

target_compile_options(
    RenegadeCW04CharacterPlacementTests
    PRIVATE
        "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>"
)

set_target_properties(
    RenegadeCW04CharacterPlacementTests
    PROPERTIES FOLDER "Renegade/Tests"
)

# Windows Studio CI builds RenegadeBridgeTests explicitly before CTest. Keep
# CW-04 in that build graph so the registered executable is always present.
add_dependencies(
    RenegadeBridgeTests
    RenegadeCW04CharacterPlacementTests
)

add_test(
    NAME RenegadeCW04CharacterPlacementTests
    COMMAND RenegadeCW04CharacterPlacementTests
)

add_test(
    NAME RenegadeCW04CharacterPlacementSourceContract
    COMMAND ${CMAKE_COMMAND}
        -DRENEGADE_SOURCE_DIR=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_LIST_DIR}/CW04CharacterPlacementSourceContract.cmake
)
