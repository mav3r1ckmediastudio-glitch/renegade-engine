#include "RuntimeScriptRuntime.h"

#include "renegade/bridge/IdentityService.h"
#include "renegade/bridge/ScriptDocumentService.h"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace
{
    namespace fs = std::filesystem;
    using namespace renegade::bridge;
    using namespace renegade::runtime;

    int Fail(const std::string& message)
    {
        std::cerr << "S5B gameplay lifecycle test failed: " << message << '\n';
        return 1;
    }

    bool WriteText(
        const fs::path& path,
        const std::string& text,
        std::string& error)
    {
        std::error_code ec;
        fs::create_directories(path.parent_path(), ec);
        if (ec)
        {
            error = ec.message();
            return false;
        }
        std::ofstream stream(path, std::ios::binary | std::ios::trunc);
        if (!stream)
        {
            error = "could not open " + path.generic_u8string();
            return false;
        }
        stream << text;
        return static_cast<bool>(stream);
    }

    bool Near(const float left, const float right) noexcept
    {
        return std::fabs(left - right) < 0.0001f;
    }
}

int main()
{
    const fs::path root = fs::temp_directory_path() /
        fs::u8path("renegade-s5b-lifecycle-" + GenerateStableId());
    const fs::path scriptPath =
        root / "Content" / "Scripts" / "lifecycle.lua";

    std::string error;
    if (!WriteText(
            scriptPath,
            "return {\n"
            " on_start=function(self)\n"
            "  assert(renegade.player.is_present())\n"
            "  local p,e=renegade.player.get_position()\n"
            "  assert(p and not e and p.x==0 and p.y==0 and p.z==0)\n"
            "  local f,e=renegade.player.get_forward()\n"
            "  assert(f and not e and f.x==0 and f.y==0 and f.z==1)\n"
            "  assert(renegade.player.get_yaw()==0)\n"
            "  assert(renegade.player.get_pitch()==0)\n"
            "  assert(renegade.input.is_down('move_forward'))\n"
            "  assert(renegade.input.is_down('sprint'))\n"
            "  assert(renegade.input.was_pressed('jump'))\n"
            "  assert(renegade.input.get_axis('move_forward')==1)\n"
            "  local bad,be=renegade.input.get_axis('not_an_action')\n"
            "  assert(bad==nil and type(be)=='string')\n"
            " end,\n"
            " on_update=function(self,dt)\n"
            "  assert(dt>0)\n"
            "  local ok,e=renegade.transform.translate_local(self.entity,{x=dt,y=0,z=0})\n"
            "  assert(ok and not e)\n"
            " end,\n"
            " on_pause=function(self) assert(renegade.player.is_present()) end,\n"
            " on_resume=function(self) assert(renegade.player.is_present()) end,\n"
            " on_reset=function(self) assert(renegade.player.is_present()) end,\n"
            " on_stop=function(self) end\n"
            "}\n",
            error))
        return Fail("write fixture: " + error);

    wi::scene::Scene scene;
    const auto playerEntity = scene.Entity_CreateTransform("Runtime Player");
    const StableId playerId = GenerateStableId();
    if (!AssignPersistentEntityId(scene, playerEntity, playerId, error))
        return Fail("assign player identity: " + error);

    const StableId projectId = GenerateStableId();
    const StableId sceneId = GenerateStableId();
    ScriptDocument document = CreateScriptDocument(
        projectId, sceneId, "Content/Scenes/S5B.wiscene", "s5b-tests");
    ScriptSourceBinding source;
    source.sourceId = GenerateStableId();
    source.sourcePath = "Content/Scripts/lifecycle.lua";
    source.presentation = ScriptPresentation::Script;
    source.apiVersion = RuntimeScriptRuntime::ApiVersion;
    source.provenance.kind = ScriptProvenanceKind::Project;
    source.provenance.contentHash = "s5b-fixture";
    if (!AddScriptAttachment(
            document,
            CreateScriptAttachment(ScriptScope::Entity, playerId, source),
            error))
        return Fail("attach fixture script: " + error);

    RuntimePlayerState player;
    player.entity = playerEntity;
    player.startEntity = playerEntity;
    player.spawnPosition = XMFLOAT3(0, 0, 0);
    player.yaw = 0.0f;
    player.pitch = 0.0f;

    GameplayInputFrame input;
    input.player.moveForward = 1.0f;
    input.player.sprintDown = true;
    input.player.jumpPressed = true;

    RuntimeScriptRuntime runtime;
    runtime.SetGameplayState(&player, &input);
    if (!runtime.StartScene(document, scene, root.generic_u8string(), error))
        return Fail("StartScene: " + error);

    if (runtime.ActiveInstanceCount() != 1 ||
        runtime.DisabledInstanceCount() != 0 ||
        !runtime.Diagnostics().empty())
        return Fail("full S5B player/input lifecycle API disabled the script");

    runtime.Update(0.25f);
    const auto* transform = scene.transforms.GetComponent(playerEntity);
    if (transform == nullptr || !Near(transform->translation_local.x, 0.25f))
        return Fail("S5B on_update did not run through the governed Runtime");

    runtime.Pause();
    runtime.Resume();
    runtime.ResetScene();
    if (runtime.IsRunning())
        return Fail("S5B reset did not terminate the scene generation");

    std::error_code ec;
    fs::remove_all(root, ec);
    return 0;
}
