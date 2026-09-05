add_executable(RenegadeS4DScriptReferenceAuthoringTests
    ${CMAKE_CURRENT_LIST_DIR}/S4DScriptReferenceAuthoringTests.cpp
)

add_executable(RenegadeS4DGlobalScriptAuthoringTests
    ${CMAKE_CURRENT_LIST_DIR}/S4DGlobalScriptAuthoringTests.cpp
)

foreach(target
    RenegadeS4DScriptReferenceAuthoringTests
    RenegadeS4DGlobalScriptAuthoringTests)
    target_link_libraries(
        ${target}
        PRIVATE
            Renegade::EngineBridge
    )
    target_compile_options(
        ${target}
        PRIVATE
            "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>"
    )
    set_target_properties(
        ${target}
        PROPERTIES
            FOLDER "Renegade/Tests"
    )
endforeach()

add_dependencies(
    RenegadeBridgeTests
    RenegadeS4DScriptReferenceAuthoringTests
    RenegadeS4DGlobalScriptAuthoringTests
)

add_test(
    NAME RenegadeS4DScriptReferenceAuthoringTests
    COMMAND RenegadeS4DScriptReferenceAuthoringTests
)

add_test(
    NAME RenegadeS4DGlobalScriptAuthoringTests
    COMMAND RenegadeS4DGlobalScriptAuthoringTests
)

add_test(
    NAME RenegadeS4DScriptReferenceAuthoringSourceContract
    COMMAND ${CMAKE_COMMAND}
        -DRENEGADE_SOURCE_DIR=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_LIST_DIR}/S4DScriptReferenceAuthoringSourceContract.cmake
)
