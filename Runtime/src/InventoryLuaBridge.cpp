#include "InventoryLuaBridge.h"

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>

namespace renegade::runtime {
namespace {
using bridge::InventoryResult;
using bridge::InventoryGameplayGateway;

const char* Message(InventoryResult result) {
    switch (result) {
    case InventoryResult::Success: return "ok";
    case InventoryResult::InvalidArgument: return "invalid_argument";
    case InventoryResult::NotFound: return "not_found";
    case InventoryResult::DuplicateId: return "duplicate_id";
    case InventoryResult::InsufficientQuantity: return "insufficient_quantity";
    case InventoryResult::CapacityExceeded: return "capacity_exceeded";
    case InventoryResult::PermissionDenied: return "permission_denied";
    case InventoryResult::InvalidSnapshot: return "invalid_snapshot";
    }
    return "unknown_error";
}
int Fail(lua_State* state, const char* reason, bool forRead=false) {
    if (forRead) lua_pushnil(state);
    else lua_pushboolean(state, 0);
    lua_pushstring(state, reason);
    return 2;
}
int Report(lua_State* state, InventoryResult result) {
    if (result != InventoryResult::Success) return Fail(state, Message(result));
    lua_pushboolean(state, 1);
    return 1;
}
InventoryGameplayGateway* Gateway(lua_State* state) {
    auto* token = static_cast<InventoryLuaContext*>(
        lua_touserdata(state, lua_upvalueindex(1)));
    return token && token->active ? token->gateway : nullptr;
}
bool String(lua_State* state, int index, std::string& out) {
    if (lua_type(state,index) != LUA_TSTRING) return false;
    std::size_t size = 0;
    const char* bytes = lua_tolstring(state,index,&size);
    if (!bytes || !size || size > 256) return false;
    out.assign(bytes,size);
    return out.find('\0') == std::string::npos;
}
bool Uint(lua_State* state,int index,std::uint32_t& out, bool allowZero=false) {
    if (!lua_isinteger(state,index)) return false;
    const auto value=lua_tointeger(state,index);
    if (value < 0 || (!allowZero && value == 0) ||
        static_cast<std::uint64_t>(value) >
            std::numeric_limits<std::uint32_t>::max()) return false;
    out=static_cast<std::uint32_t>(value);
    return true;
}
bool Weight(lua_State* state,int index,double& out) {
    if (lua_type(state,index)!=LUA_TNUMBER) return false;
    out=lua_tonumber(state,index);
    return std::isfinite(out) && out >= 0.0 && out <= 1e12;
}
void SetText(lua_State* state,const char* key,const std::string& value) {
    lua_pushlstring(state,value.data(),value.size());
    lua_setfield(state,-2,key);
}
void SetInt(lua_State* state,const char* key,std::uint64_t value) {
    lua_pushinteger(state,static_cast<lua_Integer>(value));
    lua_setfield(state,-2,key);
}
int List(lua_State* state) {
    auto* gateway=Gateway(state);
    if(!gateway)return Fail(state,"inventory_not_enabled",true);
    std::string id;
    if(!String(state,1,id))return Fail(state,"invalid_container_id",true);
    // Optional zero-based offset and page size. Never materialise an unbounded
    // Lua table; 256 stacks per call is enough for virtualised inventory UI.
    std::uint32_t offset=0, limit=64;
    if ((!lua_isnoneornil(state,2) && !Uint(state,2,offset,true)) ||
        (!lua_isnoneornil(state,3) && !Uint(state,3,limit)) ||
        limit > 256)
        return Fail(state,"invalid_inventory_page",true);
    bridge::InventoryContainer copy;
    const auto result=gateway->ReadContainer(id,copy);
    if(result!=InventoryResult::Success)
        return Fail(state,Message(result),true);
    lua_createtable(state,0,6);
    SetText(state,"id",copy.id);
    SetInt(state,"maximum_slots",copy.maximumSlots);
    lua_pushnumber(state,copy.maximumWeight);lua_setfield(state,-2,"maximum_weight");
    SetInt(state,"total_stacks",copy.stacks.size());
    const auto total=copy.stacks.size();
    const auto first=std::min(static_cast<std::size_t>(offset),total);
    const auto end=first+std::min(static_cast<std::size_t>(limit),total-first);
    SetInt(state,"next_offset",end);
    lua_createtable(state,static_cast<int>(end-first),0);
    int i=1;
    for (std::size_t k=first;k<end;++k) {
        const auto& stack=copy.stacks[k];
        lua_createtable(state,0,3);
        SetText(state,"instance_id",stack.instanceId);
        SetText(state,"definition_id",stack.definitionId);
        SetInt(state,"quantity",stack.quantity);
        lua_rawseti(state,-2,i++);
    }
    lua_setfield(state,-2,"stacks");
    return 1;
}
int Add(lua_State* state) {
    auto* gateway=Gateway(state);
    if(!gateway)return Fail(state,"inventory_not_enabled");
    std::string id, definition;
    std::uint32_t count=0;
    if(!String(state,1,id)||!String(state,2,definition)||!Uint(state,3,count))
        return Fail(state,"expected_container_definition_positive_quantity");
    return Report(state,gateway->Add(id,definition,count));
}
int Remove(lua_State* state) {
    auto* gateway=Gateway(state);
    if(!gateway)return Fail(state,"inventory_not_enabled");
    std::string id,instance;
    std::uint32_t count=0;
    if(!String(state,1,id)||!String(state,2,instance)||!Uint(state,3,count))
        return Fail(state,"expected_container_instance_positive_quantity");
    return Report(state,gateway->Remove(id,instance,count));
}
int Transfer(lua_State* state) {
    auto* gateway=Gateway(state);
    if(!gateway)return Fail(state,"inventory_not_enabled");
    std::string from,to,instance;
    std::uint32_t count=0;
    if(!String(state,1,from)||!String(state,2,to)||
       !String(state,3,instance)||!Uint(state,4,count))
        return Fail(state,"expected_source_target_instance_positive_quantity");
    return Report(state,gateway->Transfer(from,to,instance,count));
}
int Define(lua_State* state) {
    auto* gateway=Gateway(state);
    if(!gateway)return Fail(state,"inventory_not_enabled");
    bridge::InventoryItemDefinition item;
    if(!String(state,1,item.id)||!String(state,2,item.name)||
       !Uint(state,3,item.maximumStack)||!Weight(state,4,item.weightPerUnit))
        return Fail(state,"expected_id_name_max_stack_weight");
    if (!lua_isnoneornil(state,5) && lua_type(state,5)!=LUA_TSTRING)
        return Fail(state,"invalid_equipment_asset_id");
    if (lua_type(state,5)==LUA_TSTRING) {
        // An explicitly empty equipment reference is valid.
        std::size_t size=0; const char* ref=lua_tolstring(state,5,&size);
        if(size>256)return Fail(state,"invalid_equipment_asset_id");
        item.equipmentAssetId.assign(ref,size);
    }
    return Report(state,gateway->RegisterItem(std::move(item)));
}
int Create(lua_State* state) {
    auto* gateway=Gateway(state);
    if(!gateway)return Fail(state,"inventory_not_enabled");
    std::string id;
    std::uint32_t slots=0;
    double weight=0.0;
    if(!String(state,1,id) ||
       (!lua_isnoneornil(state,2) && !Uint(state,2,slots,true)) ||
       (!lua_isnoneornil(state,3) && !Weight(state,3,weight)))
        return Fail(state,"expected_id_optional_slots_optional_weight");
    return Report(state,gateway->CreateContainer(id,slots,weight));
}
void Method(lua_State* state,InventoryLuaContext* ctx,const char* name,lua_CFunction fn) {
    lua_pushlightuserdata(state,ctx);
    lua_pushcclosure(state,fn,1);
    lua_setfield(state,-2,name);
}
} // namespace

void RegisterInventoryLua(lua_State* lua,int renegadeTableIndex,InventoryLuaContext* context) {
    if(!lua || !context)return;
    const int index=lua_absindex(lua,renegadeTableIndex);
    if(!lua_istable(lua,index))return;
    lua_createtable(lua,0,7);
    lua_pushinteger(lua,1);
    lua_setfield(lua,-2,"contract_version");
    Method(lua,context,"list",List);
    Method(lua,context,"add",Add);
    Method(lua,context,"remove",Remove);
    Method(lua,context,"transfer",Transfer);
    Method(lua,context,"define",Define);
    Method(lua,context,"create",Create);
    lua_setfield(lua,index,"inventory");
}
} // namespace renegade::runtime
