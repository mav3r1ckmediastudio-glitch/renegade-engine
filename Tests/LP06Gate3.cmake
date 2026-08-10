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

# Gate 3 rollback proof. This target compiles the real BuildIdentityService.cpp
# into a test-only translation unit that redirects only UpdateResourceW so the
# second resource write can fail deterministically. BeginUpdateResourceW and
# EndUpdateResourceW remain the real Win32 APIs, exercising the production
# transaction/discard path without adding a runtime or public-API fault switch.
# Expected CTest effect: Gate 3 increases the authoritative suite to 30 tests.
add_executable(RenegadeBuildExecutableRollbackTests
    "${CMAKE_CURRENT_LIST_DIR}/BuildExecutableRollbackTests.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/BuildIdentityServiceFaultInjected.cpp"
)
target_link_libraries(
    RenegadeBuildExecutableRollbackTests
    PRIVATE
        Renegade::EngineBridge
        bcrypt
)
target_include_directories(
    RenegadeBuildExecutableRollbackTests
    PRIVATE
        "${PROJECT_SOURCE_DIR}/WickedEngine/Editor"
)
target_compile_definitions(
    RenegadeBuildExecutableRollbackTests
    PRIVATE
        UNICODE
        _UNICODE
)
target_compile_options(
    RenegadeBuildExecutableRollbackTests
    PRIVATE
        "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>"
)
set_target_properties(RenegadeBuildExecutableRollbackTests PROPERTIES
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

# Studio CI builds RenegadeBridgeTests explicitly before CTest. Keep all Gate 3
# proofs in that authoritative targeted build chain.
add_dependencies(
    RenegadeBridgeTests
    RenegadeBuildExecutableTests
    RenegadeBuildExecutableRollbackTests
    RenegadePackageBootstrapTests
)

add_test(
    NAME RenegadeBuildExecutableTests
    COMMAND RenegadeBuildExecutableTests
        "${CMAKE_CURRENT_LIST_DIR}/fixtures/lp06_gate2"
)
add_test(
    NAME RenegadeBuildExecutableRollbackTests
    COMMAND RenegadeBuildExecutableRollbackTests
)
add_test(
    NAME RenegadePackageBootstrapTests
    COMMAND RenegadePackageBootstrapTests
)
