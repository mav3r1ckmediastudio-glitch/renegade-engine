target_sources(RenegadeRuntimeBootstrap PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/src/RuntimeScriptRuntime.h
    ${CMAKE_CURRENT_LIST_DIR}/src/RuntimeScriptRuntime.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/InventoryLuaBridge.h
    ${CMAKE_CURRENT_LIST_DIR}/src/InventoryLuaBridge.cpp
)

# InventoryLuaBridge includes the Lua 5.4 C headers directly. Wicked's
# LUA target doesn't export that include directory, so RuntimeBootstrap must
# explicitly own the same pinned Lua header path used by Wicked.
target_include_directories(RenegadeRuntimeBootstrap PRIVATE
    "${PROJECT_SOURCE_DIR}/WickedEngine/WickedEngine/LUA"
)
