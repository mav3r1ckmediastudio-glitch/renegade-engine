#include "RuntimeScriptRuntime.h"
#include "RuntimeScriptEntityApi.h"

#include "renegade/bridge/GameplayEventService.h"
#include "renegade/bridge/IdentityService.h"

#include <wiLua.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace
{
    namespace fs = std::filesystem;

    constexpr const char* EntityRefMetatable = "Renegade.EntityRef";
    constexpr const char* ResourceRefMetatable = "Renegade.ResourceRef";
    constexpr std::size_t InstructionHookQuantum = 1000u;

    // Lua protected-call failures use longjmp semantics. This pointer therefore
    // only references storage owned by the outermost ProtectedCall until
    // lua_pcall itself returns and normal C++ cleanup resumes.
    thread_local std::size_t* activeInstructionBudget = nullptr;

    std::string LowerAscii(std::string value)
    {
        std::transform(
            value.begin(), value.end(), value.begin(),
            [](const unsigned char c)
            {
                return c >= 'A' && c <= 'Z'
                    ? static_cast<char>(c - 'A' + 'a')
                    : static_cast<char>(c);
            });
        return value;
    }

    std::string ModuleAlias(const std::string& projectRelativePath)
    {
        fs::path path = fs::u8path(projectRelativePath).lexically_normal();
        auto part = path.begin();
        if (part == path.end() ||
            LowerAscii(part->generic_u8string()) != "content")
            return {};
        ++part;
        if (part == path.end() ||
            LowerAscii(part->generic_u8string()) != "scripts")
            return {};
        ++part;

        fs::path relative;
        for (; part != path.end(); ++part)
            relative /= *part;
        relative.replace_extension();

        std::string alias = relative.generic_u8string();
        std::replace(alias.begin(), alias.end(), '/', '.');
        std::replace(alias.begin(), alias.end(), '\\', '.');
        return LowerAscii(std::move(alias));
    }

    void InstructionLimitHook(lua_State* state, lua_Debug*)
    {
        if (activeInstructionBudget == nullptr)
            return;
        if (*activeInstructionBudget <= InstructionHookQuantum)
        {
            *activeInstructionBudget = 0;
            luaL_error(
                state,
                "Renegade script instruction budget exceeded.");
            return;
        }
        *activeInstructionBudget -= InstructionHookQuantum;
    }

    bool ReadGovernedSource(
        const std::string& projectRoot,
        const std::string& projectRelativePath,
        std::string& source,
        std::string& error)
    {
        source.clear();
        if (!renegade::bridge::ValidateProjectScriptSource(
                projectRoot,
                projectRelativePath,
                error))
            return false;

        std::error_code ec;
        const fs::path root = fs::weakly_canonical(fs::u8path(projectRoot), ec);
        if (ec || root.empty())
        {
            error = "Could not resolve governed Lua project root: " +
                ec.message();
            return false;
        }
        const fs::path path = fs::weakly_canonical(
            root / fs::u8path(projectRelativePath), ec);
        if (ec || path.empty())
        {
            error = "Could not resolve governed Lua source: " + ec.message();
            return false;
        }

        std::ifstream stream(path, std::ios::binary);
        if (!stream)
        {
            error = "Could not open governed Lua source: " +
                projectRelativePath;
            return false;
        }
        source.assign(
            std::istreambuf_iterator<char>(stream),
            std::istreambuf_iterator<char>());
        if (stream.bad())
        {
            source.clear();
            error = "Could not read complete governed Lua source: " +
                projectRelativePath;
            return false;
        }
        if (source.find('\0') != std::string::npos)
        {
            source.clear();
            error = "Governed Lua source contains an embedded NUL byte.";
            return false;
        }
        error.clear();
        return true;
    }
}

namespace renegade::runtime
{
    struct RuntimeScriptRuntime::Impl
    {
        struct MemoryBudget
        {
            std::size_t usedBytes = 0;
            std::size_t limitBytes =
                RuntimeScriptRuntime::DefaultMemoryBudgetBytes;
        };

        struct EntityRefPayload
        {
            Impl* owner = nullptr;
            std::uint64_t generation = 0;
            char stableId[37] = {};
        };

        enum class ResourceKind : std::uint8_t
        {
            Asset,
            Animation,
            Audio,
        };

        struct ResourceRefPayload
        {
            ResourceKind kind = ResourceKind::Asset;
            char stableId[37] = {};
        };

        struct Instance
        {
            bridge::ScriptAttachment attachment;
            int environmentReference = LUA_NOREF;
            int lifecycleReference = LUA_NOREF;
            int contextReference = LUA_NOREF;
            std::unordered_map<bridge::StableId, int> moduleReferences;
            std::unordered_set<bridge::StableId> loadingModules;
            bool failed = false;
            bool hasOnStart = false;
            bool hasOnPause = false;
            bool hasOnResume = false;
            bool hasOnReset = false;
            bool hasOnStop = false;
            bool hasOnUpdate = false;
            bool hasOnEvent = false;
        };

        lua_State* lua = nullptr;
        MemoryBudget memory;
        wi::scene::Scene* scene = nullptr;
        std::string projectRoot;
        std::uint64_t generation = 0;
        bool running = false;
        bool paused = false;
        std::vector<Instance> instances;
        std::unordered_map<bridge::StableId, wi::ecs::Entity> entitiesById;
        std::vector<RuntimeScriptDiagnostic> diagnostics;
        bridge::GameplayEventService gameplayEvents;
        std::uint64_t dispatchedEventCount = 0;
        std::uint64_t eventDeliveryAttemptCount = 0;
        std::uint64_t lastEventSequence = 0;
        std::string lastEventName;
        std::string lastEventTarget;

        const bridge::RuntimePlayerState* playerState = nullptr;
        const bridge::GameplayInputFrame* gameplayInput = nullptr;

        static void* LuaAllocate(
            void* userData,
            void* pointer,
            const std::size_t oldSize,
            const std::size_t newSize)
        {
            auto* budget = static_cast<MemoryBudget*>(userData);
            if (budget == nullptr)
                return nullptr;

            if (newSize == 0)
            {
                if (pointer != nullptr)
                {
                    budget->usedBytes = oldSize <= budget->usedBytes
                        ? budget->usedBytes - oldSize
                        : 0;
                    std::free(pointer);
                }
                return nullptr;
            }

            const std::size_t accountedOld = pointer == nullptr ? 0 : oldSize;
            if (newSize > accountedOld)
            {
                const std::size_t growth = newSize - accountedOld;
                const std::size_t used =
                    std::min(budget->usedBytes, budget->limitBytes);
                if (growth > budget->limitBytes - used)
                    return nullptr;
            }

            void* replacement = std::realloc(pointer, newSize);
            if (replacement == nullptr)
                return nullptr;

            if (newSize >= accountedOld)
                budget->usedBytes += newSize - accountedOld;
            else
                budget->usedBytes -= std::min(
                    accountedOld - newSize,
                    budget->usedBytes);
            return replacement;
        }

        static int Traceback(lua_State* state)
        {
            const char* message = lua_tostring(state, 1);
            luaL_traceback(
                state,
                state,
                message == nullptr ? "Lua error" : message,
                1);
            return 1;
        }

        bool ProtectedCall(
            const int argumentCount,
            const int resultCount,
            std::string& error,
            const std::size_t instructionBudget =
                RuntimeScriptRuntime::DefaultCallbackInstructionBudget)
        {
            if (lua == nullptr)
            {
                error = "Governed Lua state is not initialized.";
                return false;
            }

            const int functionIndex = lua_gettop(lua) - argumentCount;
            lua_pushcfunction(lua, Traceback);
            lua_insert(lua, functionIndex);

            // Nested protected calls (governed require) share the outer
            // instruction budget. The hook is explicitly cleared after
            // lua_pcall returns; no C++ destructor is relied upon across Lua's
            // longjmp error path.
            const bool ownsHook = activeInstructionBudget == nullptr;
            std::size_t remaining =
                std::max(instructionBudget, InstructionHookQuantum);
            if (ownsHook)
            {
                activeInstructionBudget = &remaining;
                lua_sethook(
                    lua,
                    InstructionLimitHook,
                    LUA_MASKCOUNT,
                    static_cast<int>(InstructionHookQuantum));
            }

            const int status = lua_pcall(
                lua,
                argumentCount,
                resultCount,
                functionIndex);

            if (ownsHook)
            {
                lua_sethook(lua, nullptr, 0, 0);
                activeInstructionBudget = nullptr;
            }

            if (status != LUA_OK)
            {
                const char* message = lua_tostring(lua, -1);
                error = message == nullptr
                    ? "Unknown governed Lua failure."
                    : message;
                lua_remove(lua, functionIndex);
                return false;
            }

            lua_remove(lua, functionIndex);
            error.clear();
            return true;
        }

        void RemoveGlobal(const char* name)
        {
            lua_pushnil(lua);
            lua_setglobal(lua, name);
        }

        static int EntityRefToStringLua(lua_State* state)
        {
            auto* reference = static_cast<EntityRefPayload*>(
                luaL_testudata(state, 1, EntityRefMetatable));
            const bool valid = reference != nullptr &&
                reference->owner != nullptr &&
                reference->owner->IsLiveEntityRef(*reference);
            lua_pushstring(
                state,
                valid ? "EntityRef(valid)" : "EntityRef(stale)");
            return 1;
        }

        static int EntityRefEqualLua(lua_State* state)
        {
            auto* left = static_cast<EntityRefPayload*>(
                luaL_testudata(state, 1, EntityRefMetatable));
            auto* right = static_cast<EntityRefPayload*>(
                luaL_testudata(state, 2, EntityRefMetatable));
            const bool equal = left != nullptr && right != nullptr &&
                left->owner != nullptr &&
                left->owner == right->owner &&
                left->owner->IsLiveEntityRef(*left) &&
                left->owner->IsLiveEntityRef(*right) &&
                std::strcmp(left->stableId, right->stableId) == 0;
            lua_pushboolean(state, equal ? 1 : 0);
            return 1;
        }

        static int ResourceRefToStringLua(lua_State* state)
        {
            auto* reference = static_cast<ResourceRefPayload*>(
                luaL_testudata(state, 1, ResourceRefMetatable));
            if (reference == nullptr)
            {
                lua_pushliteral(state, "ResourceRef(invalid)");
                return 1;
            }

            switch (reference->kind)
            {
            case ResourceKind::Asset:
                lua_pushliteral(state, "AssetRef");
                break;
            case ResourceKind::Animation:
                lua_pushliteral(state, "AnimationRef");
                break;
            case ResourceKind::Audio:
                lua_pushliteral(state, "AudioRef");
                break;
            }
            return 1;
        }

        static int ResourceRefEqualLua(lua_State* state)
        {
            auto* left = static_cast<ResourceRefPayload*>(
                luaL_testudata(state, 1, ResourceRefMetatable));
            auto* right = static_cast<ResourceRefPayload*>(
                luaL_testudata(state, 2, ResourceRefMetatable));
            const bool equal = left != nullptr && right != nullptr &&
                left->kind == right->kind &&
                std::strcmp(left->stableId, right->stableId) == 0;
            lua_pushboolean(state, equal ? 1 : 0);
            return 1;
        }

        bool RegisterMetatables(std::string& error)
        {
            if (luaL_newmetatable(lua, EntityRefMetatable) != 0)
            {
                lua_pushcfunction(lua, EntityRefToStringLua);
                lua_setfield(lua, -2, "__tostring");
                lua_pushcfunction(lua, EntityRefEqualLua);
                lua_setfield(lua, -2, "__eq");
                lua_pushliteral(lua, "Renegade.EntityRef");
                lua_setfield(lua, -2, "__metatable");
            }
            lua_pop(lua, 1);

            if (luaL_newmetatable(lua, ResourceRefMetatable) != 0)
            {
                lua_pushcfunction(lua, ResourceRefToStringLua);
                lua_setfield(lua, -2, "__tostring");
                lua_pushcfunction(lua, ResourceRefEqualLua);
                lua_setfield(lua, -2, "__eq");
                lua_pushliteral(lua, "Renegade.ResourceRef");
                lua_setfield(lua, -2, "__metatable");
            }
            lua_pop(lua, 1);

            error.clear();
            return true;
        }

        bool Initialize(std::string& error)
        {
            if (lua != nullptr)
            {
                error.clear();
                return true;
            }

            memory = {};
            lua = lua_newstate(LuaAllocate, &memory);
            if (lua == nullptr)
            {
                error = "Could not create the governed Lua 5.4 state.";
                return false;
            }

            // Only deliberately governed libraries are opened. package, io,
            // os, debug and coroutine are never part of creator environments.
            luaL_requiref(lua, "_G", luaopen_base, 1);
            lua_pop(lua, 1);
            luaL_requiref(lua, LUA_TABLIBNAME, luaopen_table, 1);
            lua_pop(lua, 1);
            luaL_requiref(lua, LUA_STRLIBNAME, luaopen_string, 1);
            lua_pop(lua, 1);
            luaL_requiref(lua, LUA_MATHLIBNAME, luaopen_math, 1);
            lua_pop(lua, 1);
            luaL_requiref(lua, LUA_UTF8LIBNAME, luaopen_utf8, 1);
            lua_pop(lua, 1);

            RemoveGlobal("dofile");
            RemoveGlobal("loadfile");
            RemoveGlobal("load");
            RemoveGlobal("collectgarbage");
            RemoveGlobal("package");
            RemoveGlobal("io");
            RemoveGlobal("os");
            RemoveGlobal("debug");
            RemoveGlobal("coroutine");

            if (!RegisterMetatables(error))
            {
                lua_close(lua);
                lua = nullptr;
                memory = {};
                return false;
            }

            error.clear();
            return true;
        }

        static Impl* FromUpvalue(lua_State* state)
        {
            return static_cast<Impl*>(
                lua_touserdata(state, lua_upvalueindex(1)));
        }

        bool IsLiveEntityRef(const EntityRefPayload& reference) const noexcept
        {
            if (reference.owner != this ||
                reference.generation != generation ||
                scene == nullptr)
            {
                return false;
            }

            const auto found = entitiesById.find(reference.stableId);
            if (found == entitiesById.end())
                return false;

            // The map is a stable-ID resolver, not a liveness cache. Runtime
            // entity mutation can remove or replace an ECS entity without
            // changing the Level generation, so every creator-facing query
            // revalidates the current Scene identity before exposing success.
            return bridge::PersistentEntityId(*scene, found->second) ==
                reference.stableId;
        }

        static int EntityIsValidLua(lua_State* state)
        {
            auto* owner = FromUpvalue(state);
            auto* reference = static_cast<EntityRefPayload*>(
                luaL_testudata(state, 1, EntityRefMetatable));
            const bool valid = owner != nullptr && reference != nullptr &&
                reference->owner == owner &&
                owner->IsLiveEntityRef(*reference);
            lua_pushboolean(state, valid ? 1 : 0);
            return 1;
        }

        static int EntityEqualsLua(lua_State* state)
        {
            auto* owner = FromUpvalue(state);
            auto* left = static_cast<EntityRefPayload*>(
                luaL_testudata(state, 1, EntityRefMetatable));
            auto* right = static_cast<EntityRefPayload*>(
                luaL_testudata(state, 2, EntityRefMetatable));
            const bool equal = owner != nullptr &&
                left != nullptr && right != nullptr &&
                left->owner == owner && right->owner == owner &&
                owner->IsLiveEntityRef(*left) &&
                owner->IsLiveEntityRef(*right) &&
                std::strcmp(left->stableId, right->stableId) == 0;
            lua_pushboolean(state, equal ? 1 : 0);
            return 1;
        }

        bool ReadEntityReferenceForApi(
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

        static int PlayerIsPresentLua(lua_State* state)
        {
            auto* owner = FromUpvalue(state);
            lua_pushboolean(
                state,
                owner != nullptr && owner->playerState != nullptr &&
                    owner->playerState->IsSpawned());
            return 1;
        }

        static int PlayerGetPositionLua(lua_State* state)
        {
            auto* owner = FromUpvalue(state);
            if (owner == nullptr || owner->scene == nullptr ||
                owner->playerState == nullptr ||
                !owner->playerState->IsSpawned())
                return PushNilError(
                    state,
                    "Player API is not bound to a spawned Runtime player.");

            const auto* transform =
                owner->scene->transforms.GetComponent(owner->playerState->entity);
            if (transform == nullptr)
                return PushNilError(
                    state,
                    "Player API could not resolve the live player transform.");

            PushVector3Table(state, transform->GetPosition());
            return 1;
        }

        static int PlayerGetForwardLua(lua_State* state)
        {
            auto* owner = FromUpvalue(state);
            if (owner == nullptr || owner->playerState == nullptr ||
                !owner->playerState->IsSpawned())
                return PushNilError(
                    state,
                    "Player API is not bound to a spawned Runtime player.");

            const float yaw = owner->playerState->yaw;
            const float pitch = owner->playerState->pitch;
            const float cosPitch = std::cos(pitch);
            PushVector3Table(
                state,
                XMFLOAT3(
                    std::sin(yaw) * cosPitch,
                    -std::sin(pitch),
                    std::cos(yaw) * cosPitch));
            return 1;
        }

        static int PlayerGetYawLua(lua_State* state)
        {
            auto* owner = FromUpvalue(state);
            if (owner == nullptr || owner->playerState == nullptr ||
                !owner->playerState->IsSpawned())
                return PushNilError(
                    state,
                    "Player API is not bound to a spawned Runtime player.");
            lua_pushnumber(state, owner->playerState->yaw);
            return 1;
        }

        static int PlayerGetPitchLua(lua_State* state)
        {
            auto* owner = FromUpvalue(state);
            if (owner == nullptr || owner->playerState == nullptr ||
                !owner->playerState->IsSpawned())
                return PushNilError(
                    state,
                    "Player API is not bound to a spawned Runtime player.");
            lua_pushnumber(state, owner->playerState->pitch);
            return 1;
        }

        static bool ReadGameplayAction(
            lua_State* state,
            const int index,
            bridge::GameplayAction& action,
            std::string& error)
        {
            if (!lua_isstring(state, index))
            {
                error = "Gameplay action must be a string.";
                return false;
            }
            const char* id = lua_tostring(state, index);
            if (id == nullptr ||
                !bridge::TryParseGameplayAction(id, action))
            {
                error = "Unknown gameplay action.";
                return false;
            }
            error.clear();
            return true;
        }

        static float GameplayAxis(
            const bridge::GameplayInputFrame& frame,
            const bridge::GameplayAction action) noexcept
        {
            switch (action)
            {
            case bridge::GameplayAction::MoveForward:
                return std::clamp(frame.player.moveForward, -1.0f, 1.0f);
            case bridge::GameplayAction::MoveBackward:
                return std::clamp(-frame.player.moveForward, -1.0f, 1.0f);
            case bridge::GameplayAction::MoveLeft:
                return std::clamp(-frame.player.moveRight, -1.0f, 1.0f);
            case bridge::GameplayAction::MoveRight:
                return std::clamp(frame.player.moveRight, -1.0f, 1.0f);
            case bridge::GameplayAction::LookYaw:
                return frame.player.lookYaw;
            case bridge::GameplayAction::LookPitch:
                return frame.player.lookPitch;
            case bridge::GameplayAction::Jump:
                return frame.player.jumpPressed ? 1.0f : 0.0f;
            case bridge::GameplayAction::Sprint:
                return frame.player.sprintDown ? 1.0f : 0.0f;
            case bridge::GameplayAction::Pause:
                return frame.pausePressed ? 1.0f : 0.0f;
            case bridge::GameplayAction::Reset:
                return frame.resetPressed ? 1.0f : 0.0f;
            case bridge::GameplayAction::Count:
                break;
            }
            return 0.0f;
        }

        static int InputGetAxisLua(lua_State* state)
        {
            auto* owner = FromUpvalue(state);
            if (owner == nullptr || owner->gameplayInput == nullptr)
                return PushNilError(
                    state,
                    "Input API is not bound to the active gameplay frame.");

            bridge::GameplayAction action;
            std::string error;
            if (!ReadGameplayAction(state, 1, action, error))
                return PushNilError(state, error);
            lua_pushnumber(
                state,
                GameplayAxis(*owner->gameplayInput, action));
            return 1;
        }

        static int InputIsDownLua(lua_State* state)
        {
            auto* owner = FromUpvalue(state);
            if (owner == nullptr || owner->gameplayInput == nullptr)
                return PushFalseError(
                    state,
                    "Input API is not bound to the active gameplay frame.");

            bridge::GameplayAction action;
            std::string error;
            if (!ReadGameplayAction(state, 1, action, error))
                return PushFalseError(state, error);
            lua_pushboolean(
                state,
                std::abs(GameplayAxis(*owner->gameplayInput, action)) >
                    0.0001f);
            return 1;
        }

        static int InputWasPressedLua(lua_State* state)
        {
            auto* owner = FromUpvalue(state);
            if (owner == nullptr || owner->gameplayInput == nullptr)
                return PushFalseError(
                    state,
                    "Input API is not bound to the active gameplay frame.");

            bridge::GameplayAction action;
            std::string error;
            if (!ReadGameplayAction(state, 1, action, error))
                return PushFalseError(state, error);

            bool pressed = false;
            switch (action)
            {
            case bridge::GameplayAction::Jump:
                pressed = owner->gameplayInput->player.jumpPressed;
                break;
            case bridge::GameplayAction::Pause:
                pressed = owner->gameplayInput->pausePressed;
                break;
            case bridge::GameplayAction::Reset:
                pressed = owner->gameplayInput->resetPressed;
                break;
            default:
                break;
            }
            lua_pushboolean(state, pressed ? 1 : 0);
            return 1;
        }

        bool EnqueueGameplayEventForLua(
            lua_State* state,
            const char* instanceId,
            const bool targeted)
        {
            std::string error;
            Instance* instance = instanceId == nullptr
                ? nullptr
                : FindInstance(instanceId);
            if (!running || instance == nullptr || instance->failed)
            {
                error = "Gameplay events are not bound to an active script instance.";
            }

            const int nameIndex = targeted ? 2 : 1;
            const int payloadIndex = targeted ? 3 : 2;
            if (error.empty() &&
                (lua_type(state, nameIndex) != LUA_TSTRING ||
                 lua_type(state, payloadIndex) != LUA_TSTRING))
            {
                error = targeted
                    ? "renegade.events.send(entity,name,payload) expects string name and payload."
                    : "renegade.events.emit(name,payload) expects string name and payload.";
            }

            bridge::GameplayEvent event;
            if (error.empty())
            {
                std::size_t nameLength = 0;
                std::size_t payloadLength = 0;
                const char* name = lua_tolstring(state, nameIndex, &nameLength);
                const char* payload = lua_tolstring(state, payloadIndex, &payloadLength);
                event.name.assign(name == nullptr ? "" : name, nameLength);
                event.payload.assign(payload == nullptr ? "" : payload, payloadLength);
                if (instance->attachment.scope == bridge::ScriptScope::Entity)
                    event.senderEntityId = instance->attachment.ownerEntityId;

                if (targeted)
                {
                    auto* target = static_cast<EntityRefPayload*>(
                        luaL_testudata(state, 1, EntityRefMetatable));
                    if (target == nullptr || target->owner != this ||
                        !IsLiveEntityRef(*target))
                    {
                        error = "renegade.events.send target must be a live Renegade EntityRef.";
                    }
                    else
                    {
                        event.targetEntityId = target->stableId;
                    }
                }
            }

            const bool succeeded = error.empty() &&
                gameplayEvents.Enqueue(std::move(event), error);
            if (!succeeded)
                return PushFalseError(state, error);
            lua_pushboolean(state, 1);
            return 1;
        }

        static int EventEmitLua(lua_State* state)
        {
            auto* owner = FromUpvalue(state);
            const char* instanceId = lua_tostring(state, lua_upvalueindex(2));
            if (owner == nullptr)
            {
                lua_pushboolean(state, 0);
                lua_pushliteral(state, "Gameplay event Runtime context is unavailable.");
                return 2;
            }
            return owner->EnqueueGameplayEventForLua(state, instanceId, false);
        }

        static int EventSendLua(lua_State* state)
        {
            auto* owner = FromUpvalue(state);
            const char* instanceId = lua_tostring(state, lua_upvalueindex(2));
            if (owner == nullptr)
            {
                lua_pushboolean(state, 0);
                lua_pushliteral(state, "Gameplay event Runtime context is unavailable.");
                return 2;
            }
            return owner->EnqueueGameplayEventForLua(state, instanceId, true);
        }

        // This helper may use normal C++ objects because it always returns
        // normally. It leaves either the required module value or an error
        // string on the Lua stack for RequireLua to consume.
        bool RequireForLua(
            const char* instanceId,
            const char* requested)
        {
            std::string error;
            bool succeeded = false;
            if (instanceId == nullptr || requested == nullptr)
            {
                error = "Governed require() expects one declared module name.";
            }
            else
            {
                Instance* instance = FindInstance(instanceId);
                if (instance == nullptr)
                {
                    error =
                        "Governed require() lost its ScriptInstanceId context.";
                }
                else
                {
                    succeeded = RequireModule(*instance, requested, error);
                }
            }

            if (!succeeded)
                lua_pushlstring(lua, error.data(), error.size());
            return succeeded;
        }

        // This C callback intentionally owns only trivial locals. lua_error()
        // can therefore longjmp without skipping destructors for std::string or
        // other non-trivial C++ objects.
        static int RequireLua(lua_State* state)
        {
            Impl* owner = FromUpvalue(state);
            const char* instanceId =
                lua_tostring(state, lua_upvalueindex(2));
            const char* requested = lua_tostring(state, 1);
            if (owner != nullptr && owner->RequireForLua(instanceId, requested))
                return 1;
            if (owner == nullptr)
                lua_pushliteral(state, "Governed require() lost its Runtime context.");
            return lua_error(state);
        }

        Instance* FindInstance(const bridge::StableId& id) noexcept
        {
            const auto found = std::find_if(
                instances.begin(), instances.end(),
                [&](const Instance& instance)
                {
                    return instance.attachment.scriptInstanceId == id;
                });
            return found == instances.end() ? nullptr : &*found;
        }

        void CopyGlobalFunction(
            const int environmentIndex,
            const char* name)
        {
            lua_getglobal(lua, name);
            if (!lua_isnil(lua, -1))
                lua_setfield(lua, environmentIndex, name);
            else
                lua_pop(lua, 1);
        }

        void CopyLibrary(const int environmentIndex, const char* name)
        {
            lua_getglobal(lua, name);
            if (!lua_istable(lua, -1))
            {
                lua_pop(lua, 1);
                return;
            }

            const int sourceIndex = lua_absindex(lua, -1);
            lua_newtable(lua);
            const int destinationIndex = lua_absindex(lua, -1);
            lua_pushnil(lua);
            while (lua_next(lua, sourceIndex) != 0)
            {
                lua_pushvalue(lua, -2);
                lua_pushvalue(lua, -2);
                lua_settable(lua, destinationIndex);
                lua_pop(lua, 1);
            }
            lua_setfield(lua, environmentIndex, name);
            lua_pop(lua, 1);
        }

        int CreateEnvironment(Instance& instance)
        {
            lua_newtable(lua);
            const int environmentIndex = lua_absindex(lua, -1);

            static const char* BaseFunctions[] = {
                "assert", "error", "getmetatable", "ipairs", "next",
                "pairs", "pcall", "rawequal", "rawlen", "select",
                "setmetatable", "tonumber", "tostring", "type", "xpcall",
            };
            for (const char* name : BaseFunctions)
                CopyGlobalFunction(environmentIndex, name);

            CopyLibrary(environmentIndex, LUA_TABLIBNAME);
            CopyLibrary(environmentIndex, LUA_STRLIBNAME);
            CopyLibrary(environmentIndex, LUA_MATHLIBNAME);
            CopyLibrary(environmentIndex, LUA_UTF8LIBNAME);

            lua_pushvalue(lua, environmentIndex);
            lua_setfield(lua, environmentIndex, "_G");

            lua_newtable(lua);
            lua_pushinteger(lua, RuntimeScriptRuntime::ApiVersion);
            lua_setfield(lua, -2, "api_version");
            lua_pushliteral(lua, "5.4.8");
            lua_setfield(lua, -2, "lua_version");

            lua_newtable(lua);
            lua_pushinteger(lua, 1);
            lua_setfield(lua, -2, "contract_version");
            lua_pushlightuserdata(lua, this);
            lua_pushcclosure(lua, EntityIsValidLua, 1);
            lua_setfield(lua, -2, "is_valid");
            lua_pushlightuserdata(lua, this);
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

            lua_newtable(lua);
            lua_pushinteger(lua, 1);
            lua_setfield(lua, -2, "contract_version");
            lua_pushlightuserdata(lua, this);
            lua_pushcclosure(lua, PlayerIsPresentLua, 1);
            lua_setfield(lua, -2, "is_present");
            lua_pushlightuserdata(lua, this);
            lua_pushcclosure(lua, PlayerGetPositionLua, 1);
            lua_setfield(lua, -2, "get_position");
            lua_pushlightuserdata(lua, this);
            lua_pushcclosure(lua, PlayerGetForwardLua, 1);
            lua_setfield(lua, -2, "get_forward");
            lua_pushlightuserdata(lua, this);
            lua_pushcclosure(lua, PlayerGetYawLua, 1);
            lua_setfield(lua, -2, "get_yaw");
            lua_pushlightuserdata(lua, this);
            lua_pushcclosure(lua, PlayerGetPitchLua, 1);
            lua_setfield(lua, -2, "get_pitch");
            lua_setfield(lua, -2, "player");

            lua_newtable(lua);
            lua_pushinteger(lua, 1);
            lua_setfield(lua, -2, "contract_version");
            lua_pushlightuserdata(lua, this);
            lua_pushcclosure(lua, InputGetAxisLua, 1);
            lua_setfield(lua, -2, "get_axis");
            lua_pushlightuserdata(lua, this);
            lua_pushcclosure(lua, InputIsDownLua, 1);
            lua_setfield(lua, -2, "is_down");
            lua_pushlightuserdata(lua, this);
            lua_pushcclosure(lua, InputWasPressedLua, 1);
            lua_setfield(lua, -2, "was_pressed");
            lua_setfield(lua, -2, "input");

            lua_newtable(lua);
            lua_pushinteger(lua, 1);
            lua_setfield(lua, -2, "contract_version");
            lua_pushlightuserdata(lua, this);
            lua_pushlstring(
                lua,
                instance.attachment.scriptInstanceId.data(),
                instance.attachment.scriptInstanceId.size());
            lua_pushcclosure(lua, EventEmitLua, 2);
            lua_setfield(lua, -2, "emit");
            lua_pushlightuserdata(lua, this);
            lua_pushlstring(
                lua,
                instance.attachment.scriptInstanceId.data(),
                instance.attachment.scriptInstanceId.size());
            lua_pushcclosure(lua, EventSendLua, 2);
            lua_setfield(lua, -2, "send");
            lua_setfield(lua, -2, "events");

            lua_setfield(lua, environmentIndex, "renegade");

            lua_pushlightuserdata(lua, this);
            lua_pushlstring(
                lua,
                instance.attachment.scriptInstanceId.data(),
                instance.attachment.scriptInstanceId.size());
            lua_pushcclosure(lua, RequireLua, 2);
            lua_setfield(lua, environmentIndex, "require");

            return luaL_ref(lua, LUA_REGISTRYINDEX);
        }

        void PushEntityRef(const bridge::StableId& id)
        {
            if (!bridge::IsValidStableId(id))
            {
                lua_pushnil(lua);
                return;
            }
            auto* payload = static_cast<EntityRefPayload*>(
                lua_newuserdatauv(lua, sizeof(EntityRefPayload), 0));
            payload->owner = this;
            payload->generation = generation;
            std::memcpy(payload->stableId, id.c_str(), 36);
            payload->stableId[36] = '\0';
            luaL_getmetatable(lua, EntityRefMetatable);
            lua_setmetatable(lua, -2);
        }

        void PushResourceRef(
            const bridge::StableId& id,
            const ResourceKind kind)
        {
            if (!bridge::IsValidStableId(id))
            {
                lua_pushnil(lua);
                return;
            }
            auto* payload = static_cast<ResourceRefPayload*>(
                lua_newuserdatauv(lua, sizeof(ResourceRefPayload), 0));
            payload->kind = kind;
            std::memcpy(payload->stableId, id.c_str(), 36);
            payload->stableId[36] = '\0';
            luaL_getmetatable(lua, ResourceRefMetatable);
            lua_setmetatable(lua, -2);
        }

        void PushProperty(const bridge::ScriptPropertyValue& property)
        {
            using bridge::ScriptPropertyType;
            switch (property.type)
            {
            case ScriptPropertyType::Boolean:
                lua_pushboolean(lua, property.booleanValue ? 1 : 0);
                break;
            case ScriptPropertyType::Integer:
                lua_pushinteger(
                    lua,
                    static_cast<lua_Integer>(property.integerValue));
                break;
            case ScriptPropertyType::Float:
                lua_pushnumber(lua, property.numberValue);
                break;
            case ScriptPropertyType::String:
            case ScriptPropertyType::Enum:
                lua_pushlstring(
                    lua,
                    property.textValue.data(),
                    property.textValue.size());
                break;
            case ScriptPropertyType::Colour:
                lua_createtable(lua, 0, 4);
                lua_pushnumber(lua, property.x); lua_setfield(lua, -2, "r");
                lua_pushnumber(lua, property.y); lua_setfield(lua, -2, "g");
                lua_pushnumber(lua, property.z); lua_setfield(lua, -2, "b");
                lua_pushnumber(lua, property.w); lua_setfield(lua, -2, "a");
                break;
            case ScriptPropertyType::Vector2:
                lua_createtable(lua, 0, 2);
                lua_pushnumber(lua, property.x); lua_setfield(lua, -2, "x");
                lua_pushnumber(lua, property.y); lua_setfield(lua, -2, "y");
                break;
            case ScriptPropertyType::Vector3:
                lua_createtable(lua, 0, 3);
                lua_pushnumber(lua, property.x); lua_setfield(lua, -2, "x");
                lua_pushnumber(lua, property.y); lua_setfield(lua, -2, "y");
                lua_pushnumber(lua, property.z); lua_setfield(lua, -2, "z");
                break;
            case ScriptPropertyType::EntityReference:
                PushEntityRef(property.referenceId);
                break;
            case ScriptPropertyType::AssetReference:
                PushResourceRef(property.referenceId, ResourceKind::Asset);
                break;
            case ScriptPropertyType::Animation:
                PushResourceRef(property.referenceId, ResourceKind::Animation);
                break;
            case ScriptPropertyType::Audio:
                PushResourceRef(property.referenceId, ResourceKind::Audio);
                break;
            }
        }

        int CreateContext(const Instance& instance)
        {
            lua_newtable(lua);
            const int contextIndex = lua_absindex(lua, -1);

            if (instance.attachment.scope == bridge::ScriptScope::Entity)
                PushEntityRef(instance.attachment.ownerEntityId);
            else
                lua_pushnil(lua);
            lua_setfield(lua, contextIndex, "entity");

            lua_newtable(lua);
            for (const auto& property : instance.attachment.properties)
            {
                PushProperty(property);
                lua_setfield(lua, -2, property.name.c_str());
            }
            lua_setfield(lua, contextIndex, "properties");

            return luaL_ref(lua, LUA_REGISTRYINDEX);
        }

        void Record(
            Instance& instance,
            std::string callback,
            std::string message,
            const bool disable = true)
        {
            if (disable)
                instance.failed = true;
            diagnostics.push_back({
                instance.attachment.scriptInstanceId,
                instance.attachment.ownerEntityId,
                instance.attachment.sourcePath,
                std::move(callback),
                std::move(message),
                disable,
            });
        }

        bool CheckCallback(
            Instance& instance,
            const char* name,
            bool& present)
        {
            present = false;
            const int base = lua_gettop(lua);
            lua_rawgeti(lua, LUA_REGISTRYINDEX, instance.lifecycleReference);
            lua_getfield(lua, -1, name);
            if (lua_isnil(lua, -1))
            {
                lua_settop(lua, base);
                return true;
            }
            if (!lua_isfunction(lua, -1))
            {
                Record(
                    instance,
                    "load",
                    std::string("Lifecycle field '") + name +
                        "' must be a function when present.");
                lua_settop(lua, base);
                return false;
            }
            present = true;
            lua_settop(lua, base);
            return true;
        }

        bool LoadInstance(Instance& instance)
        {
            if (instance.attachment.unsafe)
            {
                Record(
                    instance,
                    "load",
                    "Advanced/Unsafe Lua execution is not enabled by S3.");
                return false;
            }
            if (instance.attachment.apiVersion != RuntimeScriptRuntime::ApiVersion)
            {
                Record(
                    instance,
                    "load",
                    "Script requests unsupported renegade.* API version " +
                        std::to_string(instance.attachment.apiVersion) + '.');
                return false;
            }
            if (instance.attachment.scope == bridge::ScriptScope::Entity &&
                entitiesById.find(instance.attachment.ownerEntityId) ==
                    entitiesById.end())
            {
                Record(
                    instance,
                    "load",
                    "Script owner EntityRef does not resolve in the active Scene.");
                return false;
            }

            std::string source;
            std::string sourceError;
            if (!ReadGovernedSource(
                    projectRoot,
                    instance.attachment.sourcePath,
                    source,
                    sourceError))
            {
                Record(instance, "load", std::move(sourceError));
                return false;
            }

            instance.environmentReference = CreateEnvironment(instance);
            instance.contextReference = CreateContext(instance);

            const int base = lua_gettop(lua);
            if (luaL_loadbufferx(
                    lua,
                    source.data(),
                    source.size(),
                    instance.attachment.sourcePath.c_str(),
                    "t") != LUA_OK)
            {
                const char* message = lua_tostring(lua, -1);
                Record(
                    instance,
                    "load",
                    message == nullptr
                        ? "Lua source did not compile."
                        : message);
                lua_settop(lua, base);
                return false;
            }

            lua_rawgeti(
                lua,
                LUA_REGISTRYINDEX,
                instance.environmentReference);
            const char* upvalue = lua_setupvalue(lua, -2, 1);
            if (upvalue == nullptr || std::strcmp(upvalue, "_ENV") != 0)
            {
                Record(
                    instance,
                    "load",
                    "Lua chunk does not expose the expected _ENV upvalue.");
                lua_settop(lua, base);
                return false;
            }

            std::string executionError;
            if (!ProtectedCall(0, 1, executionError))
            {
                Record(instance, "load", std::move(executionError));
                lua_settop(lua, base);
                return false;
            }
            if (!lua_istable(lua, -1))
            {
                Record(
                    instance,
                    "load",
                    "Governed script source must return one lifecycle table.");
                lua_settop(lua, base);
                return false;
            }

            instance.lifecycleReference = luaL_ref(lua, LUA_REGISTRYINDEX);
            lua_settop(lua, base);

            return CheckCallback(instance, "on_start", instance.hasOnStart) &&
                CheckCallback(instance, "on_pause", instance.hasOnPause) &&
                CheckCallback(instance, "on_resume", instance.hasOnResume) &&
                CheckCallback(instance, "on_reset", instance.hasOnReset) &&
                CheckCallback(instance, "on_stop", instance.hasOnStop) &&
                CheckCallback(instance, "on_update", instance.hasOnUpdate) &&
                CheckCallback(instance, "on_event", instance.hasOnEvent);
        }

        bool Invoke(
            Instance& instance,
            const char* callback,
            const bool present,
            const float* deltaTime = nullptr)
        {
            if (instance.failed || !present ||
                instance.lifecycleReference == LUA_NOREF ||
                instance.contextReference == LUA_NOREF)
                return !instance.failed;

            const int base = lua_gettop(lua);
            lua_rawgeti(lua, LUA_REGISTRYINDEX, instance.lifecycleReference);
            lua_getfield(lua, -1, callback);
            lua_rawgeti(lua, LUA_REGISTRYINDEX, instance.contextReference);
            int arguments = 1;
            if (deltaTime != nullptr)
            {
                lua_pushnumber(lua, *deltaTime);
                ++arguments;
            }

            std::string callbackError;
            const bool succeeded = ProtectedCall(
                arguments,
                0,
                callbackError);
            if (!succeeded)
                Record(instance, callback, std::move(callbackError));
            lua_settop(lua, base);
            return succeeded;
        }

        void PushGameplayEvent(const bridge::GameplayEvent& event)
        {
            lua_createtable(lua, 0, 5);
            lua_pushlstring(lua, event.name.data(), event.name.size());
            lua_setfield(lua, -2, "name");
            lua_pushlstring(lua, event.payload.data(), event.payload.size());
            lua_setfield(lua, -2, "payload");
            lua_pushinteger(lua, static_cast<lua_Integer>(event.sequence));
            lua_setfield(lua, -2, "sequence");
            PushEntityRef(event.senderEntityId);
            lua_setfield(lua, -2, "sender");
            PushEntityRef(event.targetEntityId);
            lua_setfield(lua, -2, "target");
        }

        bool InvokeEvent(
            Instance& instance,
            const bridge::GameplayEvent& event)
        {
            if (instance.failed || !instance.hasOnEvent ||
                instance.lifecycleReference == LUA_NOREF ||
                instance.contextReference == LUA_NOREF)
                return !instance.failed;

            const int base = lua_gettop(lua);
            lua_rawgeti(lua, LUA_REGISTRYINDEX, instance.lifecycleReference);
            lua_getfield(lua, -1, "on_event");
            lua_rawgeti(lua, LUA_REGISTRYINDEX, instance.contextReference);
            PushGameplayEvent(event);

            std::string callbackError;
            const bool succeeded = ProtectedCall(2, 0, callbackError);
            if (!succeeded)
                Record(instance, "on_event", std::move(callbackError));
            lua_settop(lua, base);
            return succeeded;
        }

        void DispatchGameplayEvents()
        {
            const std::size_t phaseCount = gameplayEvents.Size();
            for (std::size_t index = 0; index < phaseCount; ++index)
            {
                bridge::GameplayEvent event;
                if (!gameplayEvents.TryDequeue(event))
                    break;

                ++dispatchedEventCount;
                lastEventSequence = event.sequence;
                lastEventName = event.name;
                lastEventTarget = event.targetEntityId;

                for (auto& instance : instances)
                {
                    if (instance.failed || !instance.hasOnEvent)
                        continue;
                    if (!event.targetEntityId.empty() &&
                        instance.attachment.ownerEntityId != event.targetEntityId)
                        continue;
                    ++eventDeliveryAttemptCount;
                    (void)InvokeEvent(instance, event);
                }
            }
        }

        const bridge::ScriptDependency* ResolveModuleDependency(
            const Instance& instance,
            const std::string& requested,
            std::string& error) const
        {
            const std::string normalizedRequest = LowerAscii(requested);
            const bridge::ScriptDependency* resolved = nullptr;
            for (const auto& dependency : instance.attachment.dependencies)
            {
                if (dependency.kind != bridge::ScriptDependencyKind::ScriptModule)
                    continue;
                const std::string path = LowerAscii(dependency.pathHint);
                const std::string alias = ModuleAlias(dependency.pathHint);
                if (normalizedRequest != path && normalizedRequest != alias)
                    continue;
                if (resolved != nullptr && resolved->id != dependency.id)
                {
                    error = "Governed require() module name is ambiguous: " +
                        requested;
                    return nullptr;
                }
                resolved = &dependency;
            }
            if (resolved == nullptr)
            {
                error = "Governed require() rejected undeclared module: " +
                    requested;
                return nullptr;
            }
            if (resolved->pathHint.empty())
            {
                error =
                    "Governed require() dependency has no project path hint.";
                return nullptr;
            }
            error.clear();
            return resolved;
        }

        bool RequireModule(
            Instance& instance,
            const std::string& requested,
            std::string& error)
        {
            const bridge::ScriptDependency* dependency =
                ResolveModuleDependency(instance, requested, error);
            if (dependency == nullptr)
                return false;

            const auto cached =
                instance.moduleReferences.find(dependency->id);
            if (cached != instance.moduleReferences.end())
            {
                lua_rawgeti(lua, LUA_REGISTRYINDEX, cached->second);
                error.clear();
                return true;
            }
            if (!instance.loadingModules.insert(dependency->id).second)
            {
                error = "Governed require() detected a module cycle for: " +
                    requested;
                return false;
            }

            std::string source;
            if (!ReadGovernedSource(
                    projectRoot,
                    dependency->pathHint,
                    source,
                    error))
            {
                instance.loadingModules.erase(dependency->id);
                return false;
            }

            const int base = lua_gettop(lua);
            if (luaL_loadbufferx(
                    lua,
                    source.data(),
                    source.size(),
                    dependency->pathHint.c_str(),
                    "t") != LUA_OK)
            {
                const char* message = lua_tostring(lua, -1);
                error = message == nullptr
                    ? "Governed module did not compile."
                    : message;
                lua_settop(lua, base);
                instance.loadingModules.erase(dependency->id);
                return false;
            }

            lua_rawgeti(
                lua,
                LUA_REGISTRYINDEX,
                instance.environmentReference);
            const char* upvalue = lua_setupvalue(lua, -2, 1);
            if (upvalue == nullptr || std::strcmp(upvalue, "_ENV") != 0)
            {
                error =
                    "Governed module does not expose the expected _ENV upvalue.";
                lua_settop(lua, base);
                instance.loadingModules.erase(dependency->id);
                return false;
            }

            if (!ProtectedCall(0, 1, error))
            {
                lua_settop(lua, base);
                instance.loadingModules.erase(dependency->id);
                return false;
            }
            if (lua_isnil(lua, -1))
            {
                lua_pop(lua, 1);
                lua_pushboolean(lua, 1);
            }

            lua_pushvalue(lua, -1);
            const int reference = luaL_ref(lua, LUA_REGISTRYINDEX);
            instance.moduleReferences.emplace(dependency->id, reference);
            instance.loadingModules.erase(dependency->id);
            error.clear();
            return true;
        }

        void ReleaseInstance(Instance& instance)
        {
            if (lua == nullptr)
                return;
            for (const auto& module : instance.moduleReferences)
            {
                if (module.second != LUA_NOREF)
                    luaL_unref(lua, LUA_REGISTRYINDEX, module.second);
            }
            instance.moduleReferences.clear();
            instance.loadingModules.clear();
            if (instance.contextReference != LUA_NOREF)
                luaL_unref(lua, LUA_REGISTRYINDEX, instance.contextReference);
            if (instance.lifecycleReference != LUA_NOREF)
                luaL_unref(lua, LUA_REGISTRYINDEX, instance.lifecycleReference);
            if (instance.environmentReference != LUA_NOREF)
                luaL_unref(lua, LUA_REGISTRYINDEX, instance.environmentReference);
            instance.contextReference = LUA_NOREF;
            instance.lifecycleReference = LUA_NOREF;
            instance.environmentReference = LUA_NOREF;
        }

        void ReleaseInstances()
        {
            for (auto& instance : instances)
                ReleaseInstance(instance);
            instances.clear();
        }

        bool BuildEntityMap(
            wi::scene::Scene& activeScene,
            std::string& error)
        {
            entitiesById.clear();
            for (std::size_t index = 0;
                 index < activeScene.metadatas.GetCount();
                 ++index)
            {
                const wi::ecs::Entity entity =
                    activeScene.metadatas.GetEntity(index);
                const bridge::StableId id =
                    bridge::PersistentEntityId(activeScene, entity);
                if (!bridge::IsValidStableId(id))
                    continue;
                const auto inserted = entitiesById.emplace(id, entity);
                if (!inserted.second && inserted.first->second != entity)
                {
                    error =
                        "Active Scene contains a duplicate persistent entity ID: " +
                        id;
                    entitiesById.clear();
                    return false;
                }
            }
            error.clear();
            return true;
        }

        bool BeginScene(
            wi::scene::Scene& activeScene,
            std::string root,
            std::string& error)
        {
            StopScene();
            diagnostics.clear();
            gameplayEvents.Clear();
            dispatchedEventCount = 0;
            eventDeliveryAttemptCount = 0;
            lastEventSequence = 0;
            lastEventName.clear();
            lastEventTarget.clear();
            ++generation;
            scene = &activeScene;
            projectRoot = std::move(root);
            if (!BuildEntityMap(activeScene, error))
            {
                scene = nullptr;
                projectRoot.clear();
                return false;
            }
            running = true;
            paused = false;
            error.clear();
            return true;
        }

        bool StartScene(
            const bridge::ScriptDocument& document,
            wi::scene::Scene& activeScene,
            std::string root,
            std::string& error)
        {
            if (!Initialize(error))
                return false;
            if (!bridge::ValidateScriptDocument(document, error))
                return false;
            if (!BeginScene(activeScene, std::move(root), error))
                return false;

            std::vector<bridge::ScriptAttachment> ordered;
            ordered.reserve(document.attachments.size());
            for (const auto& attachment : document.attachments)
            {
                if (attachment.enabled)
                    ordered.push_back(attachment);
            }
            std::sort(
                ordered.begin(), ordered.end(),
                [](const bridge::ScriptAttachment& left,
                   const bridge::ScriptAttachment& right)
                {
                    const int leftScope =
                        left.scope == bridge::ScriptScope::Level ? 0 : 1;
                    const int rightScope =
                        right.scope == bridge::ScriptScope::Level ? 0 : 1;
                    if (leftScope != rightScope)
                        return leftScope < rightScope;
                    if (left.ownerEntityId != right.ownerEntityId)
                        return left.ownerEntityId < right.ownerEntityId;
                    if (left.order != right.order)
                        return left.order < right.order;
                    return left.scriptInstanceId < right.scriptInstanceId;
                });

            instances.reserve(ordered.size());
            for (auto& attachment : ordered)
            {
                instances.push_back({});
                instances.back().attachment = std::move(attachment);
                if (!LoadInstance(instances.back()))
                    ReleaseInstance(instances.back());
            }

            for (auto& instance : instances)
            {
                (void)Invoke(
                    instance,
                    "on_start",
                    instance.hasOnStart);
            }
            error.clear();
            return true;
        }

        void StopScene() noexcept
        {
            if (lua == nullptr)
            {
                instances.clear();
                entitiesById.clear();
                gameplayEvents.Clear();
                scene = nullptr;
                projectRoot.clear();
                running = false;
                paused = false;
                return;
            }
            if (running)
            {
                for (auto& instance : instances)
                {
                    (void)Invoke(
                        instance,
                        "on_stop",
                        instance.hasOnStop);
                }
            }
            ReleaseInstances();
            entitiesById.clear();
            gameplayEvents.Clear();
            scene = nullptr;
            projectRoot.clear();
            if (running)
                ++generation;
            running = false;
            paused = false;
        }

        void Shutdown() noexcept
        {
            StopScene();
            if (lua != nullptr)
            {
                lua_close(lua);
                lua = nullptr;
            }
            memory = {};
            diagnostics.clear();
            generation = 0;
        }
    };

    RuntimeScriptRuntime::RuntimeScriptRuntime()
        : impl_(std::make_unique<Impl>())
    {
    }

    RuntimeScriptRuntime::~RuntimeScriptRuntime()
    {
        Shutdown();
    }

    bool RuntimeScriptRuntime::Initialize(std::string& error)
    {
        return impl_->Initialize(error);
    }

    bool RuntimeScriptRuntime::StartScene(
        const bridge::ScriptDocument& document,
        wi::scene::Scene& scene,
        std::string projectRoot,
        std::string& error)
    {
        return impl_->StartScene(
            document,
            scene,
            std::move(projectRoot),
            error);
    }

    bool RuntimeScriptRuntime::StartSceneFromCompanion(
        const std::string& scenePath,
        const bridge::StableId& expectedProjectId,
        wi::scene::Scene& scene,
        std::string projectRoot,
        std::string& error)
    {
        if (!impl_->Initialize(error))
            return false;
        if (scenePath.empty() || !bridge::IsValidStableId(expectedProjectId))
        {
            error =
                "A resolved Scene path and project ID are required for creator scripting.";
            return false;
        }

        const std::string companionPath =
            bridge::ScriptDocumentPathForScene(scenePath);
        std::error_code ec;
        const fs::file_status companionStatus =
            fs::status(fs::u8path(companionPath), ec);
        if (companionStatus.type() == fs::file_type::not_found)
        {
            // A missing S2 companion is the normal compatibility case for
            // projects/scenes that have no creator scripts yet. MSVC's
            // filesystem implementation can still populate ec for not-found,
            // so recognize the status before treating ec as an I/O failure.
            ec.clear();
            return impl_->BeginScene(
                scene,
                std::move(projectRoot),
                error);
        }
        if (ec)
        {
            error = "Could not inspect the Scene script companion: " +
                ec.message();
            return false;
        }
        if (!fs::is_regular_file(companionStatus))
        {
            return impl_->BeginScene(
                scene,
                std::move(projectRoot),
                error);
        }

        bridge::DocumentEnvelope sceneEnvelope;
        if (!bridge::ReadDocumentEnvelope(
                scenePath + ".rmeta",
                sceneEnvelope,
                error))
        {
            error =
                "Scene script companion requires its owning Scene identity: " +
                error;
            return false;
        }
        if (sceneEnvelope.projectId != expectedProjectId ||
            sceneEnvelope.documentType != "scene")
        {
            error =
                "Scene script companion identity does not match the active project/Scene.";
            return false;
        }

        bridge::ScriptDocument document;
        if (!bridge::ReadScriptDocument(
                companionPath,
                expectedProjectId,
                sceneEnvelope.documentId,
                document,
                error))
            return false;

        return impl_->StartScene(
            document,
            scene,
            std::move(projectRoot),
            error);
    }

    void RuntimeScriptRuntime::SetGameplayState(
        const bridge::RuntimePlayerState* player,
        const bridge::GameplayInputFrame* input) noexcept
    {
        impl_->playerState = player;
        impl_->gameplayInput = input;
    }

    void RuntimeScriptRuntime::Update(const float dt) noexcept
    {
        if (!impl_->running || impl_->paused || impl_->lua == nullptr)
            return;
        impl_->DispatchGameplayEvents();
        const float safeDelta = std::clamp(
            std::isfinite(dt) ? dt : 0.0f,
            0.0f,
            0.25f);
        for (auto& instance : impl_->instances)
        {
            if (!instance.failed && instance.hasOnUpdate)
            {
                (void)impl_->Invoke(
                    instance,
                    "on_update",
                    true,
                    &safeDelta);
            }
        }
    }

    void RuntimeScriptRuntime::Pause() noexcept
    {
        if (!impl_->running || impl_->paused)
            return;
        for (auto& instance : impl_->instances)
        {
            (void)impl_->Invoke(
                instance,
                "on_pause",
                instance.hasOnPause);
        }
        impl_->paused = true;
    }

    void RuntimeScriptRuntime::Resume() noexcept
    {
        if (!impl_->running || !impl_->paused)
            return;
        impl_->paused = false;
        for (auto& instance : impl_->instances)
        {
            (void)impl_->Invoke(
                instance,
                "on_resume",
                instance.hasOnResume);
        }
    }

    void RuntimeScriptRuntime::ResetScene() noexcept
    {
        if (!impl_->running)
            return;
        for (auto& instance : impl_->instances)
        {
            (void)impl_->Invoke(
                instance,
                "on_reset",
                instance.hasOnReset);
        }
        impl_->StopScene();
    }

    void RuntimeScriptRuntime::StopScene() noexcept
    {
        impl_->StopScene();
    }

    void RuntimeScriptRuntime::Shutdown() noexcept
    {
        if (impl_ != nullptr)
            impl_->Shutdown();
    }

    bool RuntimeScriptRuntime::IsInitialized() const noexcept
    {
        return impl_ != nullptr && impl_->lua != nullptr;
    }

    bool RuntimeScriptRuntime::IsRunning() const noexcept
    {
        return impl_ != nullptr && impl_->running;
    }

    bool RuntimeScriptRuntime::IsPaused() const noexcept
    {
        return impl_ != nullptr && impl_->paused;
    }

    std::uint64_t RuntimeScriptRuntime::Generation() const noexcept
    {
        return impl_ == nullptr ? 0 : impl_->generation;
    }

    std::size_t RuntimeScriptRuntime::ActiveInstanceCount() const noexcept
    {
        if (impl_ == nullptr)
            return 0;
        return static_cast<std::size_t>(std::count_if(
            impl_->instances.begin(), impl_->instances.end(),
            [](const Impl::Instance& instance)
            {
                return !instance.failed;
            }));
    }

    std::size_t RuntimeScriptRuntime::DisabledInstanceCount() const noexcept
    {
        if (impl_ == nullptr)
            return 0;
        return static_cast<std::size_t>(std::count_if(
            impl_->instances.begin(), impl_->instances.end(),
            [](const Impl::Instance& instance)
            {
                return instance.failed;
            }));
    }

    std::size_t RuntimeScriptRuntime::PendingEventCount() const noexcept
    {
        return impl_ == nullptr ? 0 : impl_->gameplayEvents.Size();
    }

    std::size_t RuntimeScriptRuntime::DroppedEventCount() const noexcept
    {
        return impl_ == nullptr ? 0 : impl_->gameplayEvents.DroppedCount();
    }

    std::uint64_t RuntimeScriptRuntime::DispatchedEventCount() const noexcept
    {
        return impl_ == nullptr ? 0 : impl_->dispatchedEventCount;
    }

    std::uint64_t RuntimeScriptRuntime::EventDeliveryAttemptCount() const noexcept
    {
        return impl_ == nullptr ? 0 : impl_->eventDeliveryAttemptCount;
    }

    std::uint64_t RuntimeScriptRuntime::LastEventSequence() const noexcept
    {
        return impl_ == nullptr ? 0 : impl_->lastEventSequence;
    }

    std::string RuntimeScriptRuntime::LastEventName() const
    {
        return impl_ == nullptr ? std::string() : impl_->lastEventName;
    }

    std::string RuntimeScriptRuntime::LastEventTarget() const
    {
        return impl_ == nullptr ? std::string() : impl_->lastEventTarget;
    }

    const std::vector<RuntimeScriptDiagnostic>&
    RuntimeScriptRuntime::Diagnostics() const noexcept
    {
        return impl_->diagnostics;
    }
}
