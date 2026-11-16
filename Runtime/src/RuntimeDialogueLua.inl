// Included inside RuntimeScriptRuntime::Impl, using the existing governed VM.
static Impl *DialogueOwner(lua_State *state)
{
    auto *owner = FromUpvalue(state);
    return owner && owner->running && owner->scene && owner->dialogueApi.start ? owner : nullptr;
}
static bool DialogueString(lua_State *state, int index, std::string &value)
{
    if (lua_type(state, index) != LUA_TSTRING) return false;
    size_t length = 0;
    const auto *bytes = lua_tolstring(state, index, &length);
    if (length > 8192) return false;
    value.assign(bytes, length);
    return value.find('\0') == std::string::npos;
}
static int DialogueStartLua(lua_State *state)
{
    auto *owner = DialogueOwner(state);
    if (!owner) return PushFalseError(state, "Dialogue API is not bound to an active Level.");
    std::string key, entry, error;
    wi::ecs::Entity npc = wi::ecs::INVALID_ENTITY;
    if (lua_type(state, 1) == LUA_TSTRING)
    {
        if (!DialogueString(state, 1, key) || key.empty())
            return PushFalseError(state, "Choose a dialogue name or asset ID.");
    }
    else
    {
        RuntimeScriptEntityReference reference;
        if (!owner->ReadEntityReferenceForApi(state, 1, reference, error))
            return PushFalseError(state, error);
        RuntimeScriptEntityApi api(*owner->scene, owner->entitiesById, owner->generation);
        if (!api.Resolve(reference, npc, error)) return PushFalseError(state, error);
    }
    if (!lua_isnoneornil(state, 2) && (!DialogueString(state, 2, entry) ||
        (!entry.empty() && !bridge::IsDialogueMemoryKey(entry))))
        return PushFalseError(state, "Optional entry must be an authored line ID.");
    if (!owner->dialogueApi.start(npc, key, entry, error)) return PushFalseError(state, error);
    lua_pushboolean(state, 1);
    return 1;
}
static int DialogueStopLua(lua_State *state)
{
    auto *owner = DialogueOwner(state);
    if (!owner || !owner->dialogueApi.stop) return PushFalseError(state, "Dialogue API is unavailable.");
    std::string error;
    if (!owner->dialogueApi.stop(error)) return PushFalseError(state, error);
    lua_pushboolean(state, 1);
    return 1;
}
static int DialogueIsActiveLua(lua_State *state)
{
    auto *owner = DialogueOwner(state);
    lua_pushboolean(state, owner && owner->dialogueApi.isActive && owner->dialogueApi.isActive());
    return 1;
}
static int DialogueGetLua(lua_State *state, bool flag)
{
    auto *owner = DialogueOwner(state);
    if (!owner || !owner->dialogueApi.getValue) return PushNilError(state, "Dialogue API is unavailable.");
    std::string key, error;
    if (!DialogueString(state, 1, key) || !bridge::IsDialogueMemoryKey(key))
        return PushNilError(state, "Use a dialogue flag/value key of 1-128 letters, digits, dots, colons, dashes or underscores.");
    int value = 0;
    if (!owner->dialogueApi.getValue(key, value, error)) return PushNilError(state, error);
    if (flag) lua_pushboolean(state, value != 0);
    else lua_pushinteger(state, value);
    return 1;
}
static int DialogueSetLua(lua_State *state, bool flag)
{
    auto *owner = DialogueOwner(state);
    if (!owner || !owner->dialogueApi.setValue) return PushFalseError(state, "Dialogue API is unavailable.");
    std::string key, error;
    if (!DialogueString(state, 1, key) || !bridge::IsDialogueMemoryKey(key))
        return PushFalseError(state, "Invalid dialogue flag/value key.");
    int value = 0;
    if (flag)
    {
        if (lua_type(state, 2) != LUA_TBOOLEAN) return PushFalseError(state, "Flag values must be true or false.");
        value = lua_toboolean(state, 2) ? 1 : 0;
    }
    else
    {
        if (!lua_isinteger(state, 2)) return PushFalseError(state, "Dialogue values must be whole numbers.");
        const auto integer = lua_tointeger(state, 2);
        if (integer < std::numeric_limits<int>::min() || integer > std::numeric_limits<int>::max())
            return PushFalseError(state, "Dialogue value is outside the supported integer range.");
        value = static_cast<int>(integer);
    }
    if (!owner->dialogueApi.setValue(key, value, error)) return PushFalseError(state, error);
    lua_pushboolean(state, 1);
    return 1;
}
static int DialogueGetFlagLua(lua_State *s) { return DialogueGetLua(s, true); }
static int DialogueGetValueLua(lua_State *s) { return DialogueGetLua(s, false); }
static int DialogueSetFlagLua(lua_State *s) { return DialogueSetLua(s, true); }
static int DialogueSetValueLua(lua_State *s) { return DialogueSetLua(s, false); }
void RegisterDialogueLua()
{
    if (!dialogueApi.start) return;
    lua_newtable(lua);
    lua_pushinteger(lua, 1); lua_setfield(lua, -2, "contract_version");
    const std::pair<const char *, lua_CFunction> methods[] = {
        {"start", DialogueStartLua}, {"stop", DialogueStopLua}, {"is_active", DialogueIsActiveLua},
        {"get_flag", DialogueGetFlagLua}, {"set_flag", DialogueSetFlagLua},
        {"get_value", DialogueGetValueLua}, {"set_value", DialogueSetValueLua}};
    for (const auto &method : methods)
    {
        lua_pushlightuserdata(lua, this); lua_pushcclosure(lua, method.second, 1);
        lua_setfield(lua, -2, method.first);
    }
    lua_setfield(lua, -2, "dialogue");
}
