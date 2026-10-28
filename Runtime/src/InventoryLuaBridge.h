#pragma once

#include "renegade/bridge/InventoryGameplayGateway.h"

// The existing governed Runtime Lua 5.4 state owns the lua_State.
// This adaptor does not create a second VM, grant authority, or install globals.
struct lua_State;
namespace renegade::runtime {

// Per-creator-script binding token. The host must keep this object alive while
// Lua closures can reference it and invalidate it before scene teardown.
struct InventoryLuaContext {
    bridge::InventoryGameplayGateway* gateway = nullptr;
    bool active = false;
};

// Adds renegade.inventory to the supplied script environment's renegade table.
// The host (RuntimeScriptRuntime) must hold a distinct token per script instance.
void RegisterInventoryLua(lua_State* lua, int renegadeTableIndex,
                          InventoryLuaContext* context);

} // namespace renegade::runtime
