from pathlib import Path

path = Path("Runtime/src/RuntimeScriptRuntime.cpp")
text = path.read_text(encoding="utf-8")

old_include = '#include "RuntimeScriptRuntime.h"\n\n#include "renegade/bridge/IdentityService.h"\n'
new_include = '#include "RuntimeScriptRuntime.h"\n#include "RuntimeScriptEntityApi.h"\n\n#include "renegade/bridge/IdentityService.h"\n'
if text.count(old_include) != 1:
    raise SystemExit("S5A integration: RuntimeScriptRuntime include seam changed")
text = text.replace(old_include, new_include, 1)

marker = "        // This helper may use normal C++ objects because it always returns\n"
if text.count(marker) != 1:
    raise SystemExit("S5A integration: entity API insertion seam changed")

callbacks = r'''        bool ReadEntityReferenceForApi(
            lua_State* state,
            const int index,
            RuntimeScriptEntityReference& reference,
            std::string& error) const
        {
            auto* payload = static_cast<EntityRefPayload*>(
                luaL_testudata(state, index, EntityRefMetatable));
            if (payload == nullptr || payload->owner != this)
            {
                error = "Expected a Renegade EntityRef.";
                return false;
            }
            reference.stableId = payload->stableId;
            reference.generation = payload->generation;
            error.clear();
            return true;
        }

        static bool ReadVector3Table(
            lua_State* state,
            const int index,
            XMFLOAT3& value,
            std::string& error)
        {
            if (!lua_istable(state, index))
            {
                error = "Expected a Vector3 table with numeric x, y and z fields.";
                return false;
            }

            const int absoluteIndex = lua_absindex(state, index);
            const auto readComponent = [&](const char* field, float& component)
            {
                lua_pushstring(state, field);
                lua_rawget(state, absoluteIndex);
                if (lua_type(state, -1) != LUA_TNUMBER)
                {
                    lua_pop(state, 1);
                    error = std::string("Vector3 field '") + field +
                        "' must be numeric.";
                    return false;
                }
                component = static_cast<float>(lua_tonumber(state, -1));
                lua_pop(state, 1);
                return true;
            };

            if (!readComponent("x", value.x) ||
                !readComponent("y", value.y) ||
                !readComponent("z", value.z))
            {
                return false;
            }
            error.clear();
            return true;
        }

        static void PushVector3Table(lua_State* state, const XMFLOAT3& value)
        {
            lua_createtable(state, 0, 3);
            lua_pushnumber(state, value.x);
            lua_setfield(state, -2, "x");
            lua_pushnumber(state, value.y);
            lua_setfield(state, -2, "y");
            lua_pushnumber(state, value.z);
            lua_setfield(state, -2, "z");
        }

        static int PushNilError(lua_State* state, const std::string& error)
        {
            lua_pushnil(state);
            lua_pushlstring(state, error.data(), error.size());
            return 2;
        }

        static int PushFalseError(lua_State* state, const std::string& error)
        {
            lua_pushboolean(state, 0);
            lua_pushlstring(state, error.data(), error.size());
            return 2;
        }

        static int EntityGetNameLua(lua_State* state)
        {
            Impl* owner = FromUpvalue(state);
            if (owner == nullptr || owner->scene == nullptr)
            {
                lua_pushnil(state);
                lua_pushliteral(state, "Entity API is not bound to an active Level.");
                return 2;
            }

            RuntimeScriptEntityReference reference;
            std::string error;
            if (!owner->ReadEntityReferenceForApi(state, 1, reference, error))
                return PushNilError(state, error);

            RuntimeScriptEntityApi api(
                *owner->scene,
                owner->entitiesById,
                owner->generation);
            std::string name;
            if (!api.GetName(reference, name, error))
                return PushNilError(state, error);

            lua_pushlstring(state, name.data(), name.size());
            return 1;
        }

        static int TransformGetLocalPositionLua(lua_State* state)
        {
            Impl* owner = FromUpvalue(state);
            if (owner == nullptr || owner->scene == nullptr)
            {
                lua_pushnil(state);
                lua_pushliteral(state, "Transform API is not bound to an active Level.");
                return 2;
            }

            RuntimeScriptEntityReference reference;
            std::string error;
            if (!owner->ReadEntityReferenceForApi(state, 1, reference, error))
                return PushNilError(state, error);

            RuntimeScriptEntityApi api(
                *owner->scene,
                owner->entitiesById,
                owner->generation);
            XMFLOAT3 position;
            if (!api.GetLocalPosition(reference, position, error))
                return PushNilError(state, error);

            PushVector3Table(state, position);
            return 1;
        }

        static int TransformSetLocalPositionLua(lua_State* state)
        {
            Impl* owner = FromUpvalue(state);
            if (owner == nullptr || owner->scene == nullptr)
            {
                lua_pushboolean(state, 0);
                lua_pushliteral(state, "Transform API is not bound to an active Level.");
                return 2;
            }

            RuntimeScriptEntityReference reference;
            XMFLOAT3 position;
            std::string error;
            if (!owner->ReadEntityReferenceForApi(state, 1, reference, error) ||
                !ReadVector3Table(state, 2, position, error))
            {
                return PushFalseError(state, error);
            }

            RuntimeScriptEntityApi api(
                *owner->scene,
                owner->entitiesById,
                owner->generation);
            if (!api.SetLocalPosition(reference, position, error))
                return PushFalseError(state, error);

            lua_pushboolean(state, 1);
            return 1;
        }

        static int TransformTranslateLocalLua(lua_State* state)
        {
            Impl* owner = FromUpvalue(state);
            if (owner == nullptr || owner->scene == nullptr)
            {
                lua_pushboolean(state, 0);
                lua_pushliteral(state, "Transform API is not bound to an active Level.");
                return 2;
            }

            RuntimeScriptEntityReference reference;
            XMFLOAT3 delta;
            std::string error;
            if (!owner->ReadEntityReferenceForApi(state, 1, reference, error) ||
                !ReadVector3Table(state, 2, delta, error))
            {
                return PushFalseError(state, error);
            }

            RuntimeScriptEntityApi api(
                *owner->scene,
                owner->entitiesById,
                owner->generation);
            if (!api.TranslateLocal(reference, delta, error))
                return PushFalseError(state, error);

            lua_pushboolean(state, 1);
            return 1;
        }

'''
text = text.replace(marker, callbacks + marker, 1)

old_registration = '''            lua_pushlightuserdata(lua, this);
            lua_pushcclosure(lua, EntityEqualsLua, 1);
            lua_setfield(lua, -2, "equals");
            lua_setfield(lua, -2, "entity");
            lua_setfield(lua, environmentIndex, "renegade");
'''
new_registration = '''            lua_pushlightuserdata(lua, this);
            lua_pushcclosure(lua, EntityEqualsLua, 1);
            lua_setfield(lua, -2, "equals");
            lua_pushlightuserdata(lua, this);
            lua_pushcclosure(lua, EntityGetNameLua, 1);
            lua_setfield(lua, -2, "get_name");
            lua_setfield(lua, -2, "entity");

            lua_newtable(lua);
            lua_pushinteger(lua, 1);
            lua_setfield(lua, -2, "contract_version");
            lua_pushlightuserdata(lua, this);
            lua_pushcclosure(lua, TransformGetLocalPositionLua, 1);
            lua_setfield(lua, -2, "get_local_position");
            lua_pushlightuserdata(lua, this);
            lua_pushcclosure(lua, TransformSetLocalPositionLua, 1);
            lua_setfield(lua, -2, "set_local_position");
            lua_pushlightuserdata(lua, this);
            lua_pushcclosure(lua, TransformTranslateLocalLua, 1);
            lua_setfield(lua, -2, "translate_local");
            lua_setfield(lua, -2, "transform");

            lua_setfield(lua, environmentIndex, "renegade");
'''
if text.count(old_registration) != 1:
    raise SystemExit("S5A integration: renegade.* registration seam changed")
text = text.replace(old_registration, new_registration, 1)

path.write_text(text, encoding="utf-8")
