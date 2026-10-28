cmake_minimum_required(VERSION 3.19)
project(RenegadeInventoryCoreStandalone LANGUAGES CXX)
include(CTest)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_executable(RenegadeInventoryCoreTests
    ../InventoryCoreTests.cpp
    ../../EngineBridge/src/InventoryService.cpp
)
target_include_directories(RenegadeInventoryCoreTests PRIVATE
    ../../EngineBridge/include
)
if(MSVC)
    target_compile_options(RenegadeInventoryCoreTests PRIVATE /W4 /utf-8)
else()
    target_compile_options(RenegadeInventoryCoreTests PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_test(NAME RenegadeInventoryCoreTests COMMAND RenegadeInventoryCoreTests)
add_executable(RenegadeInventoryOverlayViewTests
    ../InventoryOverlayViewTests.cpp
    ../../EngineBridge/src/InventoryOverlayView.cpp
    ../../EngineBridge/src/InventoryService.cpp
)
target_include_directories(RenegadeInventoryOverlayViewTests PRIVATE ../../EngineBridge/include)
if(MSVC)
    target_compile_options(RenegadeInventoryOverlayViewTests PRIVATE /W4 /utf-8)
endif()
add_test(NAME RenegadeInventoryOverlayViewTests COMMAND RenegadeInventoryOverlayViewTests)
add_executable(RenegadeInventorySelectionControllerTests
    ../InventorySelectionControllerTests.cpp
    ../../EngineBridge/src/InventorySelectionController.cpp
    ../../EngineBridge/src/InventoryOverlayView.cpp
    ../../EngineBridge/src/InventoryService.cpp
)
target_include_directories(RenegadeInventorySelectionControllerTests PRIVATE
    ../../EngineBridge/include)
if(MSVC)
    target_compile_options(RenegadeInventorySelectionControllerTests PRIVATE /W4 /utf-8)
endif()
add_test(NAME RenegadeInventorySelectionControllerTests
    COMMAND RenegadeInventorySelectionControllerTests)
add_executable(RenegadeInventoryGridViewportTests
    ../InventoryGridViewportTests.cpp
    ../../EngineBridge/src/InventoryGridViewport.cpp)
target_include_directories(RenegadeInventoryGridViewportTests PRIVATE
    ../../EngineBridge/include)
if(MSVC)
    target_compile_options(RenegadeInventoryGridViewportTests PRIVATE /W4 /utf-8)
endif()
add_test(NAME RenegadeInventoryGridViewportTests
    COMMAND RenegadeInventoryGridViewportTests)
add_executable(RenegadeInventoryGridPageTests
    ../InventoryGridPageTests.cpp
    ../../EngineBridge/src/InventoryGridPage.cpp
    ../../EngineBridge/src/InventoryGridViewport.cpp
    ../../EngineBridge/src/InventoryService.cpp)
target_include_directories(RenegadeInventoryGridPageTests PRIVATE
    ../../EngineBridge/include)
if(MSVC)
    target_compile_options(RenegadeInventoryGridPageTests PRIVATE /W4 /utf-8)
endif()
add_test(NAME RenegadeInventoryGridPageTests COMMAND RenegadeInventoryGridPageTests)
add_executable(RenegadeInventoryUiInputPolicyTests
    ../InventoryUiInputPolicyTests.cpp)
target_include_directories(RenegadeInventoryUiInputPolicyTests PRIVATE ../../Runtime/src)
if(MSVC)
    target_compile_options(RenegadeInventoryUiInputPolicyTests PRIVATE /W4 /utf-8)
endif()
add_test(NAME RenegadeInventoryUiInputPolicyTests
    COMMAND RenegadeInventoryUiInputPolicyTests)
add_executable(RenegadeInventoryPointerGestureTests ../InventoryPointerGestureTests.cpp)
target_include_directories(RenegadeInventoryPointerGestureTests PRIVATE ../../Runtime/src)
if(MSVC)
    target_compile_options(RenegadeInventoryPointerGestureTests PRIVATE /W4 /utf-8)
endif()
add_test(NAME RenegadeInventoryPointerGestureTests COMMAND RenegadeInventoryPointerGestureTests)
add_executable(RenegadeInventoryAccessPolicyTests
    ../InventoryAccessPolicyTests.cpp
    ../../EngineBridge/src/InventoryAccessPolicy.cpp
    ../../EngineBridge/src/InventoryService.cpp
)
target_include_directories(RenegadeInventoryAccessPolicyTests PRIVATE ../../EngineBridge/include)
if(MSVC)
    target_compile_options(RenegadeInventoryAccessPolicyTests PRIVATE /W4 /utf-8)
endif()
add_test(NAME RenegadeInventoryAccessPolicyTests COMMAND RenegadeInventoryAccessPolicyTests)
add_executable(RenegadeInventoryTestLevelProjectionTests
    ../InventoryTestLevelSnapshotTests.cpp
    ../../EngineBridge/src/InventoryTestLevelSnapshot.cpp
    ../../EngineBridge/src/InventoryService.cpp
    ../../EngineBridge/src/InventoryAccessPolicy.cpp
)
target_include_directories(RenegadeInventoryTestLevelProjectionTests PRIVATE
    ../../EngineBridge/include)
if(MSVC)
    target_compile_options(RenegadeInventoryTestLevelProjectionTests PRIVATE /W4 /utf-8)
endif()
add_test(NAME RenegadeInventoryTestLevelProjectionTests
    COMMAND RenegadeInventoryTestLevelProjectionTests)

add_executable(RenegadeInventoryDependencyProviderTests
    ../InventoryDependencyProviderTests.cpp
    ../../EngineBridge/src/InventoryDependencyProvider.cpp
    ../../EngineBridge/src/InventoryService.cpp
    ../../EngineBridge/src/InventoryAccessPolicy.cpp
)
target_include_directories(RenegadeInventoryDependencyProviderTests PRIVATE
    ../../EngineBridge/include)
if(MSVC)
    target_compile_options(RenegadeInventoryDependencyProviderTests PRIVATE /W4 /utf-8)
endif()
add_test(NAME RenegadeInventoryDependencyProviderTests
    COMMAND RenegadeInventoryDependencyProviderTests)
add_executable(RenegadeInventoryGatewayTests
    ../InventoryGatewayTests.cpp
    ../../EngineBridge/src/InventoryService.cpp
)
target_include_directories(RenegadeInventoryGatewayTests PRIVATE ../../EngineBridge/include)
if(MSVC)
    target_compile_options(RenegadeInventoryGatewayTests PRIVATE /W4 /utf-8)
endif()
add_test(NAME RenegadeInventoryGatewayTests COMMAND RenegadeInventoryGatewayTests)

# Optional real Lua 5.4 proof, using the same Lua library as Renegade.
# Do not hardcode a developer machine's Wicked checkout or Lua binary path.
if(RENEGADE_INV_LUA_INCLUDE AND RENEGADE_INV_LUA_LIBRARY)
    add_executable(RenegadeInventoryLuaTests
        ../InventoryLuaBridgeTests.cpp
        ../../Runtime/src/InventoryLuaBridge.cpp
        ../../EngineBridge/src/InventoryService.cpp
    )
    target_include_directories(RenegadeInventoryLuaTests PRIVATE
        ../../EngineBridge/include
        "${RENEGADE_INV_LUA_INCLUDE}"
    )
    target_link_libraries(RenegadeInventoryLuaTests PRIVATE "${RENEGADE_INV_LUA_LIBRARY}")
    if(MSVC)
        target_compile_options(RenegadeInventoryLuaTests PRIVATE /W4 /utf-8)
    endif()
    add_test(NAME RenegadeInventoryLuaTests COMMAND RenegadeInventoryLuaTests)
endif()
if(RENEGADE_INV_WICKED_INCLUDE)
    # Compile (do not link) the real Runtime Lua translation unit using the
    # already-present Wicked headers in the developer's existing checkout.
    add_library(RenegadeInventoryRuntimeSyntax OBJECT
        ../../Runtime/src/RuntimeScriptRuntime.cpp
    )
    target_include_directories(RenegadeInventoryRuntimeSyntax PRIVATE
        ../../EngineBridge/include
        ../../Runtime/src
        "${RENEGADE_INV_WICKED_INCLUDE}"
        "${RENEGADE_INV_LUA_INCLUDE}"
    )
    target_compile_definitions(RenegadeInventoryRuntimeSyntax PRIVATE
        WICKED_CMAKE_BUILD UNICODE _UNICODE)
    if(MSVC)
        target_compile_options(RenegadeInventoryRuntimeSyntax PRIVATE /bigobj /utf-8)
    endif()
endif()

add_executable(RenegadeRuntimeInventorySessionTests
    ../RuntimeInventorySessionTests.cpp
    ../../Runtime/src/RuntimeInventorySession.cpp
    ../../EngineBridge/src/InventoryService.cpp
    ../../EngineBridge/src/InventoryAccessPolicy.cpp
)
target_include_directories(RenegadeRuntimeInventorySessionTests PRIVATE
    ../../EngineBridge/include ../../Runtime/src)
if(MSVC)
    target_compile_options(RenegadeRuntimeInventorySessionTests PRIVATE /W4 /utf-8)
endif()
add_test(NAME RenegadeRuntimeInventorySessionTests COMMAND RenegadeRuntimeInventorySessionTests)

if(RENEGADE_INV_WICKED_INCLUDE)
    # Compile the modified production application TU without touching the
    # active editor checkout or requiring a complete Wicked submodule checkout.
    add_library(RenegadeInventoryApplicationSyntax OBJECT
        ../../Runtime/src/RuntimeApplication.cpp
    )
    target_include_directories(RenegadeInventoryApplicationSyntax PRIVATE
        ../../EngineBridge/include ../../Runtime/src ../../ScreenRenderer/include
        "${RENEGADE_INV_WICKED_INCLUDE}" "${RENEGADE_INV_LUA_INCLUDE}"
    )
    target_compile_definitions(RenegadeInventoryApplicationSyntax PRIVATE
        WICKED_CMAKE_BUILD UNICODE _UNICODE)
    if(MSVC)
        target_compile_options(RenegadeInventoryApplicationSyntax PRIVATE /bigobj /utf-8)
    endif()
endif()

if(RENEGADE_INV_LUA_INCLUDE AND RENEGADE_INV_LUA_LIBRARY)
    add_executable(RenegadeRuntimeInventoryLuaHostTests
        ../RuntimeInventoryLuaHostTests.cpp
        ../../Runtime/src/RuntimeInventorySession.cpp
        ../../Runtime/src/InventoryLuaBridge.cpp
        ../../EngineBridge/src/InventoryService.cpp
        ../../EngineBridge/src/InventoryAccessPolicy.cpp
    )
    target_include_directories(RenegadeRuntimeInventoryLuaHostTests PRIVATE
        ../../EngineBridge/include ../../Runtime/src
        "${RENEGADE_INV_LUA_INCLUDE}")
    target_link_libraries(RenegadeRuntimeInventoryLuaHostTests PRIVATE
        "${RENEGADE_INV_LUA_LIBRARY}")
    if(MSVC)
        target_compile_options(RenegadeRuntimeInventoryLuaHostTests PRIVATE /W4 /utf-8)
    endif()
    add_test(NAME RenegadeRuntimeInventoryLuaHostTests
        COMMAND RenegadeRuntimeInventoryLuaHostTests)
endif()

if(RENEGADE_INV_WICKED_INCLUDE)
    add_library(RenegadeInventoryTestLevelSyntax OBJECT
        ../../EngineBridge/src/TestLevelSnapshotService.cpp
    )
    target_include_directories(RenegadeInventoryTestLevelSyntax PRIVATE
        ../../EngineBridge/include ../../Runtime/src
        "${RENEGADE_INV_WICKED_INCLUDE}" "${RENEGADE_INV_LUA_INCLUDE}")
    target_compile_definitions(RenegadeInventoryTestLevelSyntax PRIVATE
        WICKED_CMAKE_BUILD UNICODE _UNICODE)
    if(MSVC)
        target_compile_options(RenegadeInventoryTestLevelSyntax PRIVATE /bigobj /utf-8)
    endif()
endif()

if(RENEGADE_INV_LUA_INCLUDE AND RENEGADE_INV_LUA_LIBRARY)
    add_executable(RenegadeInventoryInteractionLuaTests
        ../InventoryInteractionLuaTests.cpp
        ../../Runtime/src/InventoryLuaBridge.cpp
        ../../EngineBridge/src/InventoryService.cpp
    )
    target_include_directories(RenegadeInventoryInteractionLuaTests PRIVATE
        ../../EngineBridge/include ../../Runtime/src
        "${RENEGADE_INV_LUA_INCLUDE}")
    target_compile_definitions(RenegadeInventoryInteractionLuaTests PRIVATE
        RENEGADE_INV_EXAMPLE_PATH="${CMAKE_CURRENT_LIST_DIR}/../../Examples/Inventory/InteractTransfer.lua")
    target_link_libraries(RenegadeInventoryInteractionLuaTests PRIVATE
        "${RENEGADE_INV_LUA_LIBRARY}")
    if(MSVC)
        target_compile_options(RenegadeInventoryInteractionLuaTests PRIVATE /W4 /utf-8)
    endif()
    add_test(NAME RenegadeInventoryInteractionLuaTests
        COMMAND RenegadeInventoryInteractionLuaTests)
endif()

if(RENEGADE_INV_WICKED_INCLUDE)
    add_library(RenegadeInventoryBuildProjectSyntax OBJECT
        ../../EngineBridge/src/WindowsGameBuildProjectService.cpp)
    target_include_directories(RenegadeInventoryBuildProjectSyntax PRIVATE
        ../../EngineBridge/include "${RENEGADE_INV_WICKED_INCLUDE}")
    target_compile_definitions(RenegadeInventoryBuildProjectSyntax PRIVATE
        WICKED_CMAKE_BUILD UNICODE _UNICODE)
    if(MSVC)
        target_compile_options(RenegadeInventoryBuildProjectSyntax PRIVATE /bigobj /utf-8)
    endif()
endif()
