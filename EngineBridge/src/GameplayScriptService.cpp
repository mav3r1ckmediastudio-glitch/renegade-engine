#include "renegade/bridge/GameplayScriptService.h"

#include "renegade/bridge/AudioService.h"
#include "renegade/bridge/IdentityService.h"
#include "renegade/bridge/PhysicsService.h"

#include <wiLua.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <utility>

namespace
{
    namespace fs = std::filesystem;
    constexpr std::uintmax_t MaximumScriptBytes = 1024u * 1024u;

    std::string Lower(std::string value)
    {
        std::transform(value.begin(), value.end(), value.begin(),
            [](const unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return value;
    }

    bool IsInside(const fs::path& candidate, const fs::path& root)
    {
        const fs::path relative = candidate.lexically_relative(root);
        if (relative.empty() || relative.is_absolute())
            return candidate == root;
        return relative.begin() == relative.end() || *relative.begin() != "..";
    }

    bool IsSafeRelativeScriptPath(const fs::path& path)
    {
        if (path.empty() || path.is_absolute() || path.has_root_name() ||
            path.has_root_directory() || Lower(path.extension().generic_u8string()) != ".lua")
        {
            return false;
        }
        for (const auto& part : path)
        {
            if (part == "..")
                return false;
        }
        const auto normalized = path.lexically_normal();
        auto part = normalized.begin();
        if (part == normalized.end() || *part++ != "Content" ||
            part == normalized.end() || *part != "Scripts")
        {
            return false;
        }
        return true;
    }

    bool ReadScriptText(
        const fs::path& path,
        std::string& text,
        std::string& error)
    {
        std::error_code ec;
        if (!fs::is_regular_file(path, ec) || ec || fs::is_symlink(path, ec) || ec)
        {
            error = "Gameplay script is not a regular non-symlink file.";
            return false;
        }
        const auto size = fs::file_size(path, ec);
        if (ec || size == 0 || size > MaximumScriptBytes)
        {
            error = "Gameplay script must contain 1 byte to 1 MiB of Lua source.";
            return false;
        }
        std::ifstream stream(path, std::ios::binary);
        if (!stream)
        {
            error = "Gameplay script could not be opened.";
            return false;
        }
        text.assign(std::istreambuf_iterator<char>(stream), {});
        if (stream.bad() || text.size() != size ||
            text.find('\0') != std::string::npos)
        {
            error = "Gameplay script could not be read as complete Lua source.";
            text.clear();
            return false;
        }
        error.clear();
        return true;
    }

    bool EntityExists(
        const wi::scene::Scene& scene,
        const wi::ecs::Entity entity) noexcept
    {
        return entity != wi::ecs::INVALID_ENTITY &&
            (scene.names.Contains(entity) || scene.metadatas.Contains(entity) ||
                scene.scripts.Contains(entity));
    }

    wi::ecs::Entity ResolveStableEntity(
        const wi::scene::Scene& scene,
        const std::string& id) noexcept
    {
        if (!renegade::bridge::IsValidStableId(id))
            return wi::ecs::INVALID_ENTITY;
        for (std::size_t index = 0; index < scene.metadatas.GetCount(); ++index)
        {
            const auto entity = scene.metadatas.GetEntity(index);
            if (renegade::bridge::PersistentEntityId(scene, entity) == id)
                return entity;
        }
        return wi::ecs::INVALID_ENTITY;
    }

    void PushVector(lua_State* L, const XMFLOAT3& value)
    {
        lua_createtable(L, 0, 3);
        lua_pushnumber(L, value.x); lua_setfield(L, -2, "x");
        lua_pushnumber(L, value.y); lua_setfield(L, -2, "y");
        lua_pushnumber(L, value.z); lua_setfield(L, -2, "z");
    }
}

namespace renegade::bridge
{
    bool IsGameplayScript(
        const wi::scene::Scene& scene,
        const wi::ecs::Entity entity) noexcept
    {
        const auto* metadata = scene.metadatas.GetComponent(entity);
        return scene.scripts.Contains(entity) && metadata != nullptr &&
            metadata->string_values.has(GameplayScriptMetadataKey) &&
            metadata->string_values.get(GameplayScriptMetadataKey) ==
                GameplayScriptMetadataVersion &&
            metadata->string_values.has(GameplayScriptPathMetadataKey);
    }

    GameplayScriptState CaptureGameplayScript(
        const wi::scene::Scene& scene,
        const wi::ecs::Entity entity) noexcept
    {
        GameplayScriptState result;
        const auto* script = scene.scripts.GetComponent(entity);
        const auto* metadata = scene.metadatas.GetComponent(entity);
        if (script == nullptr || metadata == nullptr ||
            !IsGameplayScript(scene, entity))
        {
            return result;
        }
        result.projectRelativePath = metadata->string_values.get(
            GameplayScriptPathMetadataKey);
        result.enabled = !metadata->bool_values.has(
            GameplayScriptEnabledMetadataKey) ||
            metadata->bool_values.get(GameplayScriptEnabledMetadataKey);
        return result;
    }

    void PrepareGameplayScriptsForRuntime(wi::scene::Scene& scene) noexcept
    {
        for (std::size_t index = 0; index < scene.scripts.GetCount(); ++index)
        {
            const auto entity = scene.scripts.GetEntity(index);
            if (!IsGameplayScript(scene, entity))
                continue;
            auto& script = scene.scripts[index];
            script._flags &= ~wi::scene::ScriptComponent::PLAYING;
            script._flags &= ~wi::scene::ScriptComponent::PLAY_ONCE;
        }
    }

    bool ResolveGameplayScriptPath(
        const std::string& projectRoot,
        const std::string& projectRelativePath,
        std::string& absolutePath,
        std::string& error)
    {
        absolutePath.clear();
        if (projectRoot.empty() ||
            !IsSafeRelativeScriptPath(fs::u8path(projectRelativePath)))
        {
            error = "Gameplay scripts must use a project-relative Content/Scripts/*.lua path.";
            return false;
        }
        std::error_code ec;
        const fs::path root = fs::weakly_canonical(fs::u8path(projectRoot), ec);
        if (ec || root.empty() || !fs::is_directory(root, ec) || ec)
        {
            error = "Gameplay script project root could not be resolved.";
            return false;
        }
        const fs::path candidate = fs::weakly_canonical(
            root / fs::u8path(projectRelativePath), ec);
        const fs::path scriptsRoot = fs::weakly_canonical(
            root / "Content" / "Scripts", ec);
        if (ec || candidate.empty() || scriptsRoot.empty() ||
            !IsInside(scriptsRoot, root) ||
            !IsInside(candidate, scriptsRoot))
        {
            error = "Gameplay script resolves outside Content/Scripts.";
            return false;
        }
        std::string ignored;
        if (!ReadScriptText(candidate, ignored, error))
            return false;
        absolutePath = candidate.generic_u8string();
        error.clear();
        return true;
    }

    bool ImportGameplayScript(
        const std::string& projectRoot,
        const std::string& sourcePath,
        std::string& projectRelativePath,
        std::string& error)
    {
        projectRelativePath.clear();
        if (projectRoot.empty() || sourcePath.empty())
        {
            error = "A project and Lua source file are required.";
            return false;
        }
        std::error_code ec;
        const fs::path root = fs::weakly_canonical(fs::u8path(projectRoot), ec);
        const fs::path source = fs::weakly_canonical(fs::u8path(sourcePath), ec);
        if (ec || root.empty() || source.empty() ||
            Lower(source.extension().generic_u8string()) != ".lua")
        {
            error = "The selected gameplay script must be a readable .lua file.";
            return false;
        }
        std::string sourceText;
        if (!ReadScriptText(source, sourceText, error))
            return false;

        const fs::path folder = root / "Content" / "Scripts";
        fs::create_directories(folder, ec);
        if (ec)
        {
            error = "Content/Scripts could not be created.";
            return false;
        }
        const fs::path canonicalFolder = fs::weakly_canonical(folder, ec);
        if (ec || canonicalFolder.empty() ||
            !IsInside(canonicalFolder, root))
        {
            error = "Content/Scripts could not be resolved inside the project.";
            return false;
        }

        fs::path destination = source;
        if (!IsInside(source, canonicalFolder))
        {
            destination = canonicalFolder / source.filename();
            for (int suffix = 2; fs::exists(destination, ec) && !ec; ++suffix)
            {
                if (suffix > 9999)
                {
                    error = "A unique gameplay script filename could not be chosen.";
                    return false;
                }
                destination = canonicalFolder /
                    fs::u8path(source.stem().generic_u8string() + "_" +
                        std::to_string(suffix) + ".lua");
            }
            if (ec)
            {
                error = "Gameplay script destination could not be inspected.";
                return false;
            }
            fs::copy_file(source, destination, fs::copy_options::none, ec);
            if (ec)
            {
                error = "Gameplay script could not be copied into Content/Scripts.";
                return false;
            }
        }

        projectRelativePath = destination.lexically_relative(root)
            .lexically_normal().generic_u8string();
        std::string resolved;
        if (!ResolveGameplayScriptPath(
                root.generic_u8string(), projectRelativePath, resolved, error))
        {
            if (destination != source)
            {
                std::error_code ignored;
                fs::remove(destination, ignored);
            }
            projectRelativePath.clear();
            return false;
        }
        error.clear();
        return true;
    }

    bool ValidateGameplayScriptSyntax(
        const std::string& projectRoot,
        const std::string& projectRelativePath,
        lua_State* state,
        std::string& error)
    {
        if (state == nullptr)
        {
            error = "Wicked Lua is not initialized for script validation.";
            return false;
        }
        std::string absolute;
        if (!ResolveGameplayScriptPath(
                projectRoot, projectRelativePath, absolute, error))
        {
            return false;
        }
        std::string source;
        if (!ReadScriptText(fs::u8path(absolute), source, error))
            return false;
        const int base = lua_gettop(state);
        if (luaL_loadbuffer(
                state,
                source.data(),
                source.size(),
                projectRelativePath.c_str()) != LUA_OK)
        {
            const char* message = lua_tostring(state, -1);
            error = message == nullptr
                ? "Gameplay script contains invalid Lua syntax."
                : message;
            lua_settop(state, base);
            return false;
        }
        lua_settop(state, base);
        error.clear();
        return true;
    }

    CreateGameplayScriptCommand::CreateGameplayScriptCommand(
        wi::scene::Scene& scene,
        std::string projectRoot,
        GameplayScriptState state)
        : scene_(&scene)
        , projectRoot_(std::move(projectRoot))
        , state_(std::move(state))
    {
    }

    bool CreateGameplayScriptCommand::Execute()
    {
        if (scene_ == nullptr ||
            !IsSafeRelativeScriptPath(fs::u8path(state_.projectRelativePath)))
            return false;
        if (hasSnapshot_)
        {
            if (EntityExists(*scene_, entity_))
                return false;
            snapshot_.SetReadModeAndResetPos(true);
            wi::ecs::EntitySerializer serializer;
            serializer.allow_remap = false;
            return scene_->Entity_Serialize(snapshot_, serializer) == entity_;
        }

        std::string nativePath;
        std::string pathError;
        if (!ResolveGameplayScriptPath(
                projectRoot_,
                state_.projectRelativePath,
                nativePath,
                pathError))
        {
            return false;
        }

        std::string baseName = fs::u8path(state_.projectRelativePath)
            .stem().generic_u8string();
        if (baseName.empty())
            baseName = "Gameplay Script";
        std::string name = baseName;
        for (int suffix = 2;; ++suffix)
        {
            bool collision = false;
            for (std::size_t index = 0; index < scene_->names.GetCount(); ++index)
            {
                if (scene_->names[index].name == name)
                {
                    collision = true;
                    break;
                }
            }
            if (!collision)
                break;
            name = baseName + " " + std::to_string(suffix);
        }
        entity_ = scene_->Entity_CreateTransform(name);
        if (entity_ == wi::ecs::INVALID_ENTITY)
            return false;
        scene_->transforms.Remove(entity_);
        auto& script = scene_->scripts.Create(entity_);
        // Wicked expects an absolute resource filename while the live scene is
        // in memory. Its serializer then writes a scene-relative reference and
        // reconstructs the absolute path on load. Renegade's metadata below is
        // the stable project-relative authority exposed to Studio and Runtime.
        script.filename = nativePath;
        script._flags &= ~wi::scene::ScriptComponent::PLAYING;
        script._flags &= ~wi::scene::ScriptComponent::PLAY_ONCE;
        auto* metadata = scene_->metadatas.GetComponent(entity_);
        if (metadata == nullptr)
            metadata = &scene_->metadatas.Create(entity_);
        metadata->string_values.set(
            GameplayScriptMetadataKey, GameplayScriptMetadataVersion);
        metadata->string_values.set(
            GameplayScriptPathMetadataKey, state_.projectRelativePath);
        metadata->bool_values.set(
            GameplayScriptEnabledMetadataKey, state_.enabled);

        // Test Level can snapshot a newly authored, not-yet-saved scene. Give
        // the carrier its stable lifecycle identity at creation time instead
        // of relying on the later whole-document save validation pass.
        std::string identityError;
        if (!AssignNewPersistentEntityId(*scene_, entity_, identityError))
        {
            scene_->Entity_Remove(entity_);
            entity_ = wi::ecs::INVALID_ENTITY;
            return false;
        }

        snapshot_.SetReadModeAndResetPos(false);
        wi::ecs::EntitySerializer serializer;
        scene_->Entity_Serialize(snapshot_, serializer, entity_);
        hasSnapshot_ = true;
        return true;
    }

    void CreateGameplayScriptCommand::Undo()
    {
        if (scene_ != nullptr && EntityExists(*scene_, entity_))
            scene_->Entity_Remove(entity_);
    }

    wi::ecs::Entity CreateGameplayScriptCommand::CreatedEntity() const noexcept
    {
        return entity_;
    }

    SetGameplayScriptEnabledCommand::SetGameplayScriptEnabledCommand(
        wi::scene::Scene& scene,
        const wi::ecs::Entity entity,
        const bool enabled)
        : scene_(&scene)
        , entity_(entity)
        , before_(CaptureGameplayScript(scene, entity).enabled)
        , after_(enabled)
    {
    }

    bool SetGameplayScriptEnabledCommand::Execute()
    {
        return before_ != after_ && Apply(after_);
    }

    void SetGameplayScriptEnabledCommand::Undo()
    {
        (void)Apply(before_);
    }

    bool SetGameplayScriptEnabledCommand::Apply(const bool enabled) noexcept
    {
        if (scene_ == nullptr || !IsGameplayScript(*scene_, entity_))
            return false;
        auto* metadata = scene_->metadatas.GetComponent(entity_);
        if (metadata == nullptr)
            return false;
        metadata->bool_values.set(GameplayScriptEnabledMetadataKey, enabled);
        return true;
    }

    struct GameplayScriptRuntime::Impl
    {
        struct Instance
        {
            wi::ecs::Entity entity = wi::ecs::INVALID_ENTITY;
            std::string entityId;
            std::string path;
            int tableReference = LUA_NOREF;
            bool failed = false;
        };

        lua_State* lua = nullptr;
        wi::scene::Scene* scene = nullptr;
        const GameplayInputFrame* input = nullptr;
        const RuntimePlayerState* player = nullptr;
        std::string projectRoot;
        std::vector<Instance> instances;
        std::vector<GameplayScriptDiagnostic> diagnostics;
        bool running = false;
        bool paused = false;

        static Impl* Self(lua_State* L)
        {
            return static_cast<Impl*>(lua_touserdata(L, lua_upvalueindex(1)));
        }

        static void PushBool(lua_State* L, const bool value)
        {
            lua_pushboolean(L, value ? 1 : 0);
        }

        static int EntityExistsLua(lua_State* L)
        {
            auto* self = Self(L);
            const char* id = luaL_checkstring(L, 1);
            PushBool(L, self != nullptr && self->scene != nullptr &&
                ResolveStableEntity(*self->scene, id) != wi::ecs::INVALID_ENTITY);
            return 1;
        }

        static int EntityFindLua(lua_State* L)
        {
            auto* self = Self(L);
            const char* name = luaL_checkstring(L, 1);
            if (self == nullptr || self->scene == nullptr)
            {
                lua_pushnil(L);
                return 1;
            }
            wi::ecs::Entity found = wi::ecs::INVALID_ENTITY;
            for (std::size_t index = 0; index < self->scene->names.GetCount(); ++index)
            {
                if (self->scene->names[index].name != name)
                    continue;
                if (found != wi::ecs::INVALID_ENTITY)
                {
                    lua_pushnil(L);
                    return 1;
                }
                found = self->scene->names.GetEntity(index);
            }
            const std::string id = PersistentEntityId(*self->scene, found);
            if (!IsValidStableId(id)) lua_pushnil(L);
            else lua_pushlstring(L, id.data(), id.size());
            return 1;
        }

        static int EntityNativeIdLua(lua_State* L)
        {
            auto* self = Self(L);
            const char* id = luaL_checkstring(L, 1);
            const auto entity = self != nullptr && self->scene != nullptr
                ? ResolveStableEntity(*self->scene, id)
                : wi::ecs::INVALID_ENTITY;
            if (entity == wi::ecs::INVALID_ENTITY) lua_pushnil(L);
            else lua_pushinteger(L, static_cast<lua_Integer>(entity));
            return 1;
        }

        static int EntityPositionLua(lua_State* L)
        {
            auto* self = Self(L);
            const char* id = luaL_checkstring(L, 1);
            const auto entity = self != nullptr && self->scene != nullptr
                ? ResolveStableEntity(*self->scene, id)
                : wi::ecs::INVALID_ENTITY;
            const auto* transform = self != nullptr && self->scene != nullptr
                ? self->scene->transforms.GetComponent(entity) : nullptr;
            if (transform == nullptr) lua_pushnil(L);
            else PushVector(L, transform->GetPosition());
            return 1;
        }

        static bool ActionValue(
            const GameplayInputFrame& frame,
            const std::string& action,
            float& value,
            bool& pressed,
            bool& down)
        {
            value = 0.0f; pressed = false; down = false;
            if (action == "move_forward") value = std::max(0.0f, frame.player.moveForward);
            else if (action == "move_backward") value = std::max(0.0f, -frame.player.moveForward);
            else if (action == "move_right") value = std::max(0.0f, frame.player.moveRight);
            else if (action == "move_left") value = std::max(0.0f, -frame.player.moveRight);
            else if (action == "look_yaw") value = frame.player.lookYaw;
            else if (action == "look_pitch") value = frame.player.lookPitch;
            else if (action == "jump") pressed = frame.player.jumpPressed;
            else if (action == "sprint") down = frame.player.sprintDown;
            else if (action == "pause") pressed = frame.pausePressed;
            else if (action == "reset") pressed = frame.resetPressed;
            else return false;
            down = down || pressed || std::abs(value) > 0.00001f;
            return true;
        }

        static int InputValueLua(lua_State* L)
        {
            auto* self = Self(L);
            float value = 0.0f; bool pressed = false; bool down = false;
            if (self == nullptr || self->input == nullptr ||
                !ActionValue(*self->input, luaL_checkstring(L, 1), value, pressed, down))
            {
                lua_pushnil(L);
            }
            else lua_pushnumber(L, value);
            return 1;
        }

        static int InputPressedLua(lua_State* L)
        {
            auto* self = Self(L);
            float value = 0.0f; bool pressed = false; bool down = false;
            PushBool(L, self != nullptr && self->input != nullptr &&
                ActionValue(*self->input, luaL_checkstring(L, 1), value, pressed, down) &&
                pressed);
            return 1;
        }

        static int InputDownLua(lua_State* L)
        {
            auto* self = Self(L);
            float value = 0.0f; bool pressed = false; bool down = false;
            PushBool(L, self != nullptr && self->input != nullptr &&
                ActionValue(*self->input, luaL_checkstring(L, 1), value, pressed, down) &&
                down);
            return 1;
        }

        static int PlayerSpawnedLua(lua_State* L)
        {
            auto* self = Self(L);
            PushBool(L, self != nullptr && self->player != nullptr &&
                self->player->IsSpawned());
            return 1;
        }

        static int PlayerPositionLua(lua_State* L)
        {
            auto* self = Self(L);
            if (self == nullptr || self->scene == nullptr || self->player == nullptr ||
                !self->player->IsSpawned())
            {
                lua_pushnil(L);
                return 1;
            }
            XMFLOAT3 position = self->player->spawnPosition;
            if (!GetPhysicsPosition(*self->scene, self->player->entity, position))
            {
                const auto* transform = self->scene->transforms.GetComponent(
                    self->player->entity);
                if (transform != nullptr)
                    position = transform->GetPosition();
            }
            PushVector(L, position);
            return 1;
        }

        static int AudioPlayLua(lua_State* L)
        {
            auto* self = Self(L);
            const char* id = luaL_checkstring(L, 1);
            const auto entity = self != nullptr && self->scene != nullptr
                ? ResolveStableEntity(*self->scene, id)
                : wi::ecs::INVALID_ENTITY;
            auto* sound = self != nullptr && self->scene != nullptr
                ? self->scene->sounds.GetComponent(entity) : nullptr;
            const bool accepted = sound != nullptr &&
                IsRenegadeSoundSource(*self->scene, entity) &&
                sound->soundResource.IsValid();
            if (accepted) sound->Play();
            PushBool(L, accepted);
            return 1;
        }

        static int AudioStopLua(lua_State* L)
        {
            auto* self = Self(L);
            const char* id = luaL_checkstring(L, 1);
            const auto entity = self != nullptr && self->scene != nullptr
                ? ResolveStableEntity(*self->scene, id)
                : wi::ecs::INVALID_ENTITY;
            auto* sound = self != nullptr && self->scene != nullptr
                ? self->scene->sounds.GetComponent(entity) : nullptr;
            const bool accepted = sound != nullptr &&
                IsRenegadeSoundSource(*self->scene, entity);
            if (accepted) sound->Stop();
            PushBool(L, accepted);
            return 1;
        }

        void AddFunction(
            const char* name,
            lua_CFunction function)
        {
            lua_pushlightuserdata(lua, this);
            lua_pushcclosure(lua, function, 1);
            lua_setfield(lua, -2, name);
        }

        void RegisterNamespace()
        {
            lua_getglobal(lua, "renegade");
            if (!lua_istable(lua, -1))
            {
                lua_pop(lua, 1);
                lua_newtable(lua);
                lua_pushvalue(lua, -1);
                lua_setglobal(lua, "renegade");
            }

            lua_newtable(lua);
            lua_pushinteger(lua, 1); lua_setfield(lua, -2, "contract_version");
            AddFunction("exists", EntityExistsLua);
            AddFunction("find", EntityFindLua);
            AddFunction("native_id", EntityNativeIdLua);
            AddFunction("position", EntityPositionLua);
            lua_setfield(lua, -2, "entity");

            lua_newtable(lua);
            lua_pushinteger(lua, 1); lua_setfield(lua, -2, "contract_version");
            AddFunction("value", InputValueLua);
            AddFunction("pressed", InputPressedLua);
            AddFunction("down", InputDownLua);
            lua_setfield(lua, -2, "input");

            lua_newtable(lua);
            lua_pushinteger(lua, 1); lua_setfield(lua, -2, "contract_version");
            AddFunction("is_spawned", PlayerSpawnedLua);
            AddFunction("position", PlayerPositionLua);
            lua_setfield(lua, -2, "player");

            lua_newtable(lua);
            lua_pushinteger(lua, 1); lua_setfield(lua, -2, "contract_version");
            AddFunction("play", AudioPlayLua);
            AddFunction("stop", AudioStopLua);
            lua_setfield(lua, -2, "audio");

            lua_pop(lua, 1);
        }

        void Record(
            Instance& instance,
            std::string callback,
            std::string message)
        {
            diagnostics.push_back({
                instance.entityId, instance.path,
                std::move(callback), std::move(message)});
        }

        bool Invoke(
            Instance& instance,
            const char* callback,
            const float* dt = nullptr)
        {
            if (instance.failed || instance.tableReference == LUA_NOREF)
                return false;
            const int base = lua_gettop(lua);
            lua_rawgeti(lua, LUA_REGISTRYINDEX, instance.tableReference);
            lua_getfield(lua, -1, callback);
            if (lua_isnil(lua, -1))
            {
                lua_settop(lua, base);
                return true;
            }
            if (!lua_isfunction(lua, -1))
            {
                Record(instance, callback, "Lifecycle field is not a function.");
                instance.failed = true;
                lua_settop(lua, base);
                return false;
            }
            lua_pushvalue(lua, -2);
            lua_createtable(lua, 0, 2);
            lua_pushlstring(lua, instance.entityId.data(), instance.entityId.size());
            lua_setfield(lua, -2, "entity_id");
            int argumentCount = 2;
            if (dt != nullptr)
            {
                lua_pushnumber(lua, *dt);
                ++argumentCount;
            }
            if (lua_pcall(lua, argumentCount, 0, 0) != LUA_OK)
            {
                const char* message = lua_tostring(lua, -1);
                Record(instance, callback,
                    message == nullptr ? "Unknown Lua callback failure." : message);
                instance.failed = true;
                lua_settop(lua, base);
                return false;
            }
            lua_settop(lua, base);
            return true;
        }

        void ReleaseInstances()
        {
            if (lua != nullptr)
            {
                for (auto& instance : instances)
                {
                    if (instance.tableReference != LUA_NOREF)
                    {
                        luaL_unref(lua, LUA_REGISTRYINDEX, instance.tableReference);
                        instance.tableReference = LUA_NOREF;
                    }
                }
            }
            instances.clear();
        }
    };

    GameplayScriptRuntime::GameplayScriptRuntime()
        : impl_(std::make_unique<Impl>())
    {
    }

    GameplayScriptRuntime::~GameplayScriptRuntime()
    {
        Stop();
    }

    bool GameplayScriptRuntime::Start(
        wi::scene::Scene& scene,
        std::string projectRoot,
        const GameplayInputFrame& input,
        const RuntimePlayerState& player,
        std::string& error)
    {
        return Start(scene, std::move(projectRoot), input, player,
            wi::lua::GetLuaState(), error);
    }

    bool GameplayScriptRuntime::Start(
        wi::scene::Scene& scene,
        std::string projectRoot,
        const GameplayInputFrame& input,
        const RuntimePlayerState& player,
        lua_State* state,
        std::string& error)
    {
        Stop();
        impl_->diagnostics.clear();
        if (state == nullptr)
        {
            error = "Wicked Lua is not initialized.";
            return false;
        }
        impl_->lua = state;
        impl_->scene = &scene;
        impl_->input = &input;
        impl_->player = &player;
        impl_->projectRoot = std::move(projectRoot);
        impl_->RegisterNamespace();

        std::vector<Impl::Instance> candidates;
        for (std::size_t index = 0; index < scene.scripts.GetCount(); ++index)
        {
            const auto entity = scene.scripts.GetEntity(index);
            if (!IsGameplayScript(scene, entity))
                continue;
            auto stateValue = CaptureGameplayScript(scene, entity);
            if (!stateValue.enabled)
                continue;
            candidates.push_back({
                entity,
                PersistentEntityId(scene, entity),
                std::move(stateValue.projectRelativePath)});
        }
        std::sort(candidates.begin(), candidates.end(),
            [](const Impl::Instance& left, const Impl::Instance& right)
            {
                if (left.entityId != right.entityId)
                    return left.entityId < right.entityId;
                return left.entity < right.entity;
            });

        for (auto& instance : candidates)
        {
            if (!IsValidStableId(instance.entityId))
            {
                impl_->Record(instance, "load",
                    "Gameplay script entity is missing a valid persistent ID.");
                continue;
            }
            std::string absolute;
            std::string resolveError;
            if (!ResolveGameplayScriptPath(
                    impl_->projectRoot, instance.path, absolute, resolveError))
            {
                impl_->Record(instance, "load", std::move(resolveError));
                continue;
            }
            std::string source;
            if (!ReadScriptText(fs::u8path(absolute), source, resolveError))
            {
                impl_->Record(instance, "load", std::move(resolveError));
                continue;
            }
            const int base = lua_gettop(state);
            if (luaL_loadbuffer(
                    state, source.data(), source.size(), instance.path.c_str()) != LUA_OK ||
                lua_pcall(state, 0, 1, 0) != LUA_OK)
            {
                const char* message = lua_tostring(state, -1);
                impl_->Record(instance, "load",
                    message == nullptr ? "Unknown Lua load failure." : message);
                lua_settop(state, base);
                continue;
            }
            if (!lua_istable(state, -1))
            {
                impl_->Record(instance, "load",
                    "Gameplay script must return one lifecycle table.");
                lua_settop(state, base);
                continue;
            }
            instance.tableReference = luaL_ref(state, LUA_REGISTRYINDEX);
            lua_settop(state, base);
            impl_->instances.push_back(std::move(instance));
        }

        impl_->running = true;
        impl_->paused = false;
        for (auto& instance : impl_->instances)
            (void)impl_->Invoke(instance, "on_start");
        error.clear();
        return true;
    }

    void GameplayScriptRuntime::Update(
        const float dt,
        const GameplayInputFrame& input,
        const RuntimePlayerState& player) noexcept
    {
        if (!impl_->running || impl_->paused)
            return;
        impl_->input = &input;
        impl_->player = &player;
        const float safeDt = std::clamp(
            std::isfinite(dt) ? dt : 0.0f, 0.0f, 0.1f);
        for (auto& instance : impl_->instances)
            (void)impl_->Invoke(instance, "on_update", &safeDt);
    }

    void GameplayScriptRuntime::Pause() noexcept
    {
        if (!impl_->running || impl_->paused)
            return;
        for (auto& instance : impl_->instances)
            (void)impl_->Invoke(instance, "on_pause");
        impl_->paused = true;
    }

    void GameplayScriptRuntime::Resume() noexcept
    {
        if (!impl_->running || !impl_->paused)
            return;
        impl_->paused = false;
        for (auto& instance : impl_->instances)
            (void)impl_->Invoke(instance, "on_resume");
    }

    void GameplayScriptRuntime::Reset() noexcept
    {
        if (!impl_->running)
            return;
        for (auto& instance : impl_->instances)
            (void)impl_->Invoke(instance, "on_reset");
        Stop();
    }

    void GameplayScriptRuntime::Stop() noexcept
    {
        if (impl_ == nullptr)
            return;
        if (impl_->running)
        {
            for (auto& instance : impl_->instances)
                (void)impl_->Invoke(instance, "on_stop");
        }
        impl_->ReleaseInstances();
        impl_->running = false;
        impl_->paused = false;
        impl_->scene = nullptr;
        impl_->input = nullptr;
        impl_->player = nullptr;
        impl_->projectRoot.clear();
        impl_->lua = nullptr;
    }

    bool GameplayScriptRuntime::IsRunning() const noexcept
    {
        return impl_->running;
    }

    bool GameplayScriptRuntime::IsPaused() const noexcept
    {
        return impl_->paused;
    }

    std::size_t GameplayScriptRuntime::ActiveScriptCount() const noexcept
    {
        return impl_->instances.size();
    }

    const std::vector<GameplayScriptDiagnostic>&
    GameplayScriptRuntime::Diagnostics() const noexcept
    {
        return impl_->diagnostics;
    }
}