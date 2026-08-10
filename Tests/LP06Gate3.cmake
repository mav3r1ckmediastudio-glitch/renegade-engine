add_executable(RenegadeBuildExecutableTests
    "${CMAKE_CURRENT_LIST_DIR}/BuildExecutableTests.cpp"
)
target_link_libraries(
    RenegadeBuildExecutableTests
    PRIVATE
        Renegade::EngineBridge
        version
        bcrypt
)
target_compile_definitions(
    RenegadeBuildExecutableTests
    PRIVATE
        UNICODE
        _UNICODE
)
target_compile_options(
    RenegadeBuildExecutableTests
    PRIVATE
        "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>"
)
set_target_properties(RenegadeBuildExecutableTests PROPERTIES
    FOLDER "Renegade/Tests"
)

add_executable(RenegadePackageBootstrapTests
    "${CMAKE_CURRENT_LIST_DIR}/PackageBootstrapTests.cpp"
)
target_link_libraries(
    RenegadePackageBootstrapTests
    PRIVATE
        Renegade::RuntimeBootstrap
)
target_compile_definitions(
    RenegadePackageBootstrapTests
    PRIVATE
        UNICODE
        _UNICODE
)
target_compile_options(
    RenegadePackageBootstrapTests
    PRIVATE
        "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>"
)
set_target_properties(RenegadePackageBootstrapTests PROPERTIES
    FOLDER "Renegade/Tests"
)

# Studio CI builds RenegadeBridgeTests explicitly before CTest. Keep both
# Gate 3 proofs in that authoritative targeted build chain.
add_dependencies(
    RenegadeBridgeTests
    RenegadeBuildExecutableTests
    RenegadePackageBootstrapTests
)

add_test(
    NAME RenegadeBuildExecutableTests
    COMMAND RenegadeBuildExecutableTests
        "${CMAKE_CURRENT_LIST_DIR}/fixtures/lp06_gate2"
)
add_test(
    NAME RenegadePackageBootstrapTests
    COMMAND RenegadePackageBootstrapTests
)
