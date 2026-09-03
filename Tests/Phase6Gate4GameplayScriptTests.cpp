#include "renegade/bridge/AudioService.h"
#include "renegade/bridge/GameplayScriptService.h"
#include "renegade/bridge/IdentityService.h"
#include "renegade/bridge/PhysicsLuaService.h"

#include <wiLua.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace
{
    namespace fs = std::filesystem;
    int failures = 0;

    void Check(const bool condition, const char* message)
    {
        if (condition)
            return;
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }

    void WriteText(const fs::path& path, const std::string& text)
    {
        fs::create_directories(path.parent_path());
        std::ofstream stream(path, std::ios::binary);
        stream << text;
    }

    void CreateScript(
        wi::scene::Scene& scene,
        const char* name,
        const char* relativePath,
        const char* id)
    {
        const auto entity = scene.Entity_CreateTransform(name);
        scene.transforms.Remove(entity);
        auto& script = scene.scripts.Create(entity);
        script.filename = relativePath;
        script._flags &= ~wi::scene::ScriptComponent::PLAYING;
        auto& metadata = scene.metadatas.Create(entity);
        metadata.string_values.set(
            renegade::bridge::GameplayScriptMetadataKey,
            renegade::bridge::GameplayScriptMetadataVersion);
        metadata.string_values.set(
            renegade::bridge::GameplayScriptPathMetadataKey,
            relativePath);
        metadata.bool_values.set(
            renegade::bridge::GameplayScriptEnabledMetadataKey,
            true);
        std::string error;
        Check(renegade::bridge::AssignPersistentEntityId(
            scene, entity, id, error), "script fixture could not receive stable ID");
    }

    std::vector<std::string> EventLog(lua_State* L)
    {
        std::vector<std::string> result;
        lua_getglobal(L, "gate4_events");
        if (!lua_istable(L, -1))
        {
            lua_pop(L, 1);
            return result;
        }
        const auto count = lua_rawlen(L, -1);
        for (std::size_t index = 1; index <= count; ++index)
        {
            lua_rawgeti(L, -1, static_cast<lua_Integer>(index));
            const char* value = lua_tostring(L, -1);
            result.emplace_back(value == nullptr ? "" : value);
            lua_pop(L, 1);
        }
        lua_pop(L, 1);
        return result;
    }

    bool GlobalBool(lua_State* L, const char* name)
    {
        lua_getglobal(L, name);
        const bool value = lua_toboolean(L, -1) != 0;
        lua_pop(L, 1);
        return value;
    }
}

int main()
{
    using namespace renegade::bridge;
    const fs::path root = fs::temp_directory_path() /
        "renegade_phase6_gate4_gameplay_script_tests";
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(root / "Content" / "Scripts", ec);
    Check(!ec, "test project could not be created");

    const fs::path external = root.parent_path() / "gate4_external.lua";
    WriteText(external, "return {}\n");
    std::string imported;
    std::string error;
    Check(ImportGameplayScript(
        root.generic_u8string(), external.generic_u8string(), imported, error),
        "external Lua source was not imported into the project");
    Check(imported == "Content/Scripts/gate4_external.lua",
        "import did not produce the canonical project-relative script path");
    std::string resolved;
    Check(ResolveGameplayScriptPath(
        root.generic_u8string(), imported, resolved, error),
        "imported script did not resolve inside Content/Scripts");
    std::string rejectedResolved;
    Check(!ResolveGameplayScriptPath(
        root.generic_u8string(), "../gate4_external.lua", rejectedResolved, error),
        "script path traversal was accepted");

    {
        wi::scene::Scene commandScene;
        CreateGameplayScriptCommand command(
            commandScene,
            root.generic_u8string(),
            GameplayScriptState{imported, true});
        Check(command.Execute(), "script creation command failed");
        const auto entity = command.CreatedEntity();
        Check(IsGameplayScript(commandScene, entity),
            "script command did not create the governed metadata marker");
        Check(IsValidStableId(PersistentEntityId(commandScene, entity)),
            "script command did not create a stable lifecycle identity");
        const auto* native = commandScene.scripts.GetComponent(entity);
        Check(native != nullptr &&
                fs::u8path(native->filename).lexically_normal() ==
                    fs::u8path(resolved).lexically_normal(),
            "script command did not assign Wicked's resolved native filename");
        Check(CaptureGameplayScript(commandScene, entity).projectRelativePath ==
                imported,
            "script command did not persist its project-relative authority");
        Check(native != nullptr && !native->IsPlaying(),
            "script command left Wicked's competing per-frame lifecycle enabled");
        SetGameplayScriptEnabledCommand disable(commandScene, entity, false);
        Check(disable.Execute(), "script Enabled command did not execute");
        Check(!CaptureGameplayScript(commandScene, entity).enabled,
            "script Enabled command did not persist disabled state");
        disable.Undo();
        Check(CaptureGameplayScript(commandScene, entity).enabled,
            "script Enabled command Undo did not restore state");
        command.Undo();
        Check(!IsGameplayScript(commandScene, entity),
            "script command Undo did not remove its carrier");
        Check(command.Execute(), "script command Redo failed");
        Check(IsGameplayScript(commandScene, entity),
            "script command Redo lost governed metadata");
    }

    const char* targetId = "10000000-0000-4000-8000-000000000010";
    const char* audioId = "10000000-0000-4000-8000-000000000020";
    const std::string commonPrefix =
        "gate4_events = gate4_events or {}\n"
        "local function event(v) table.insert(gate4_events, v) end\n";
    WriteText(root / "Content" / "Scripts" / "a.lua", commonPrefix +
        "return {\n"
        " on_start=function(self,ctx)\n"
        "  gate4_api_ok = renegade.entity.exists(ctx.entity_id) and "
        "renegade.entity.find('Target') == '" + targetId + "' and "
        "renegade.entity.position('" + targetId + "').x == 3 and "
        "renegade.audio.stop('" + audioId + "') and "
        "renegade.physics.contract_version == 1 and "
        "not renegade.player.is_spawned()\n"
        "  event('a:start') end,\n"
        " on_update=function(self,ctx,dt) if dt > 0 then event('a:update') end end,\n"
        " on_pause=function() event('a:pause') end,\n"
        " on_resume=function() event('a:resume') end,\n"
        " on_reset=function() event('a:reset') end,\n"
        " on_stop=function() event('a:stop') end,\n"
        "}\n");
    WriteText(root / "Content" / "Scripts" / "b.lua", commonPrefix +
        "return {\n"
        " on_start=function() event('b:start') end,\n"
        " on_update=function() event('b:update') end,\n"
        " on_pause=function() event('b:pause') end,\n"
        " on_resume=function() event('b:resume') end,\n"
        " on_reset=function() event('b:reset') end,\n"
        " on_stop=function() event('b:stop') end,\n"
        "}\n");
    WriteText(root / "Content" / "Scripts" / "broken.lua", commonPrefix +
        "return {\n"
        " on_start=function() event('broken:start') end,\n"
        " on_update=function() error('isolated failure') end,\n"
        " on_pause=function() event('broken:pause') end,\n"
        "}\n");
    WriteText(root / "Content" / "Scripts" / "invalid.lua",
        "return { on_start = function( }\n");

    wi::scene::Scene scene;
    const auto target = scene.Entity_CreateTransform("Target");
    auto* targetTransform = scene.transforms.GetComponent(target);
    targetTransform->Translate(XMFLOAT3(3.0f, 4.0f, 5.0f));
    targetTransform->UpdateTransform();
    Check(AssignPersistentEntityId(scene, target, targetId, error),
        "target fixture could not receive stable ID");

    const auto audio = scene.Entity_CreateTransform("Audio");
    scene.sounds.Create(audio);
    auto& audioMetadata = scene.metadatas.Create(audio);
    audioMetadata.string_values.set(
        AudioSourceMetadataKey, AudioSourceMetadataVersion);
    Check(AssignPersistentEntityId(scene, audio, audioId, error),
        "audio fixture could not receive stable ID");

    CreateScript(scene, "A", "Content/Scripts/a.lua",
        "10000000-0000-4000-8000-000000000001");
    CreateScript(scene, "B", "Content/Scripts/b.lua",
        "10000000-0000-4000-8000-000000000002");
    CreateScript(scene, "Broken", "Content/Scripts/broken.lua",
        "10000000-0000-4000-8000-000000000003");

    lua_State* L = luaL_newstate();
    Check(L != nullptr, "isolated Lua state could not be created");
    if (L != nullptr)
    {
        luaL_openlibs(L);
        Check(ValidateGameplayScriptSyntax(
            root.generic_u8string(), "Content/Scripts/a.lua", L, error),
            "valid lifecycle script failed Studio syntax validation");
        Check(!ValidateGameplayScriptSyntax(
            root.generic_u8string(), "Content/Scripts/invalid.lua", L, error),
            "invalid lifecycle script passed Studio syntax validation");
        Check(BindPhysicsLua(scene, L),
            "accepted renegade.physics namespace could not bind");
        GameplayInputFrame input;
        RuntimePlayerState player;
        GameplayScriptRuntime runtime;
        Check(runtime.Start(
            scene, root.generic_u8string(), input, player, L, error),
            "gameplay lifecycle did not start");
        Check(runtime.ActiveScriptCount() == 3,
            "gameplay lifecycle did not load every governed script");
        Check(GlobalBool(L, "gate4_api_ok"),
            "stable entity/input/player/audio/physics API contract failed");

        runtime.Update(0.016f, input, player);
        runtime.Update(0.016f, input, player);
        Check(runtime.Diagnostics().size() == 1,
            "one failing script was not isolated into one diagnostic");
        if (!runtime.Diagnostics().empty())
        {
            Check(runtime.Diagnostics().front().callback == "on_update" &&
                runtime.Diagnostics().front().message.find("isolated failure") !=
                    std::string::npos,
                "callback diagnostic lost its phase or Lua message");
        }
        runtime.Pause();
        Check(runtime.IsPaused(), "script lifecycle did not pause");
        runtime.Update(0.016f, input, player);
        runtime.Resume();
        runtime.Reset();
        Check(!runtime.IsRunning(), "script lifecycle reset did not stop instances");

        const std::vector<std::string> expected = {
            "a:start", "b:start", "broken:start",
            "a:update", "b:update", "a:update", "b:update",
            "a:pause", "b:pause", "a:resume", "b:resume",
            "a:reset", "b:reset", "a:stop", "b:stop",
        };
        Check(EventLog(L) == expected,
            "lifecycle callbacks were not deterministic or pause-safe");

        UnbindPhysicsLua(&scene);
        lua_close(L);
    }

    fs::remove_all(root, ec);
    fs::remove(external, ec);
    if (failures != 0)
        return 1;
    std::cout << "PASS: Phase 6 Gate 4 governed Lua gameplay lifecycle\n";
    return 0;
}
