#include "RuntimeScriptRuntime.h"

#include "renegade/bridge/AudioService.h"
#include "renegade/bridge/IdentityService.h"
#include "renegade/bridge/ScriptDocumentService.h"
#include "renegade/bridge/ScriptMetadataService.h"

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
        std::cerr << "S7 stock Action runtime test failed: " << message << '\n';
        return 1;
    }

    bool Near(const float left, const float right) noexcept
    {
        return std::fabs(left - right) < 0.0001f;
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
            error = "could not create fixture directory: " + ec.message();
            return false;
        }
        std::ofstream stream(path, std::ios::binary | std::ios::trunc);
        if (!stream)
        {
            error = "could not open fixture source";
            return false;
        }
        stream << text;
        if (!stream)
        {
            error = "could not write fixture source";
            return false;
        }
        error.clear();
        return true;
    }
}

int main()
{
    using namespace renegade::bridge;
    using namespace renegade::runtime;

    const fs::path root = fs::temp_directory_path() /
        fs::u8path("renegade-s7-actions-" + GenerateStableId());
    std::error_code ec;
    const std::string sourcePath = "Content/Scripts/s7_runtime_seams.lua";
    const fs::path scriptPath = root / fs::u8path(sourcePath);

    std::string error;
    if (!WriteText(
            scriptPath,
            "return {\n"
            " on_start=function(self)\n"
            "  local world,worlderr=renegade.transform.get_world_position(self.entity)\n"
            "  assert(world and not worlderr and world.x==1 and world.y==2 and world.z==3)\n"
            "  local scale,scaleerr=renegade.transform.get_local_scale(self.entity)\n"
            "  assert(scale and not scaleerr and scale.x==1 and scale.y==1 and scale.z==1)\n"
            "  local ok,seterr=renegade.transform.set_local_scale(self.entity,{x=2,y=3,z=4})\n"
            "  assert(ok and not seterr)\n"
            "  local played,playerr=renegade.audio.play(self.properties.source)\n"
            "  assert(played==false and type(playerr)=='string')\n"
            "  local stopped,stoperr=renegade.audio.stop(self.properties.source)\n"
            "  assert(stopped and not stoperr)\n"
            " end,\n"
            " on_update=function(self,dt)\n"
            "  assert(type(dt)=='number')\n"
            "  local shown,prompterr=renegade.ui.show_prompt('Press E to test')\n"
            "  assert(shown and not prompterr)\n"
            "  if renegade.input.was_pressed('interact') then\n"
            "   local ok,err=renegade.events.emit('s7.interact','')\n"
            "   assert(ok and not err)\n"
            "  end\n"
            " end\n"
            "}\n",
            error))
    {
        return Fail(error);
    }

    wi::scene::Scene scene;
    const wi::ecs::Entity actionEntity = scene.Entity_CreateTransform("S7 Action Host");
    auto* actionTransform = scene.transforms.GetComponent(actionEntity);
    if (actionTransform == nullptr)
        return Fail("fixture action entity has no transform");
    actionTransform->translation_local = XMFLOAT3(1.0f, 2.0f, 3.0f);
    actionTransform->SetDirty();
    actionTransform->UpdateTransform();

    const StableId actionId = GenerateStableId();
    if (!AssignPersistentEntityId(scene, actionEntity, actionId, error))
        return Fail("assign action identity: " + error);

    const wi::ecs::Entity soundEntity = scene.Entity_CreateSound(
        "S7 Sound Source", {}, XMFLOAT3(0.0f, 0.0f, 0.0f));
    if (soundEntity == wi::ecs::INVALID_ENTITY)
        return Fail("could not create fixture Sound Source entity");
    if (!scene.sounds.Contains(soundEntity))
        scene.sounds.Create(soundEntity);
    if (!ApplySoundSource(scene, soundEntity, SoundSourceState{}, error))
        return Fail("mark fixture as Renegade Sound Source: " + error);

    const StableId soundId = GenerateStableId();
    if (!AssignPersistentEntityId(scene, soundEntity, soundId, error))
        return Fail("assign Sound Source identity: " + error);
    if (!IsRenegadeSoundSource(scene, soundEntity))
        return Fail("fixture did not become an authored Renegade Sound Source");

    const StableId projectId = GenerateStableId();
    const StableId sceneId = GenerateStableId();
    ScriptDocument document = CreateScriptDocument(
        projectId,
        sceneId,
        "Content/Scenes/S7.wiscene",
        "s7-stock-actions-tests");

    ScriptSourceBinding source;
    source.sourceId = GenerateStableId();
    source.sourcePath = sourcePath;
    source.presentation = ScriptPresentation::Action;
    source.apiVersion = RuntimeScriptRuntime::ApiVersion;
    source.provenance.kind = ScriptProvenanceKind::Project;
    source.provenance.contentHash = "s7-runtime-seam-fixture";

    ScriptAttachment attachment = CreateScriptAttachment(
        ScriptScope::Entity,
        actionId,
        source);
    ScriptPropertyValue audioSource;
    audioSource.name = "source";
    audioSource.type = ScriptPropertyType::EntityReference;
    audioSource.referenceId = soundId;
    attachment.properties.push_back(audioSource);
    if (!AddScriptAttachment(document, std::move(attachment), error))
        return Fail("attach fixture Action: " + error);

    GameplayInputFrame input;
    input.interactPressed = true;

    RuntimeScriptRuntime runtime;
    runtime.SetGameplayState(nullptr, &input);
    if (!runtime.StartScene(document, scene, root.generic_u8string(), error))
        return Fail("StartScene: " + error);
    if (runtime.ActiveInstanceCount() != 1 ||
        runtime.DisabledInstanceCount() != 0 ||
        !runtime.Diagnostics().empty())
    {
        const std::string detail = runtime.Diagnostics().empty()
            ? std::string{}
            : ": " + runtime.Diagnostics().front().message;
        return Fail("new generic APIs disabled the Action" + detail);
    }

    actionTransform = scene.transforms.GetComponent(actionEntity);
    if (actionTransform == nullptr ||
        !Near(actionTransform->scale_local.x, 2.0f) ||
        !Near(actionTransform->scale_local.y, 3.0f) ||
        !Near(actionTransform->scale_local.z, 4.0f))
    {
        return Fail("governed Lua did not apply the S7 local-scale mutation");
    }

    runtime.Update(1.0f / 60.0f);
    if (runtime.PendingEventCount() != 1)
        return Fail("Interact did not enqueue the stock Action gameplay event");
    if (runtime.CurrentPrompt() != "Press E to test")
        return Fail("governed Lua prompt did not reach the Runtime presentation seam");

    input.interactPressed = false;
    runtime.Update(1.0f / 60.0f);
    if (runtime.DispatchedEventCount() != 1 ||
        runtime.LastEventName() != "s7.interact" ||
        runtime.DisabledInstanceCount() != 0 ||
        !runtime.Diagnostics().empty())
    {
        return Fail("Interact event did not survive deterministic Runtime dispatch");
    }

    runtime.StopScene();
    if (!runtime.CurrentPrompt().empty())
        return Fail("Runtime retained a stale prompt after stopping the Level");

    // Execute the real shipped Sliding Door Action, not a token-only fixture.
    const std::string doorSourcePath = "Content/Scripts/Stock/Door.lua";
    const fs::path doorSource =
        fs::u8path(RENEGADE_SOURCE_DIR) /
        "Library" / "Scripts" / "RenegadeStockActions" / "Door.lua";
    const fs::path copiedDoor = root / fs::u8path(doorSourcePath);
    fs::create_directories(copiedDoor.parent_path(), ec);
    ec.clear();
    fs::copy_file(
        doorSource,
        copiedDoor,
        fs::copy_options::overwrite_existing,
        ec);
    if (ec)
        return Fail("copy real Sliding Door Action: " + ec.message());

    wi::scene::Scene doorScene;
    const wi::ecs::Entity doorEntity = doorScene.Entity_CreateTransform("Door");
    const wi::ecs::Entity playerEntity = doorScene.Entity_CreateTransform("Player");
    const StableId doorId = GenerateStableId();
    const StableId playerId = GenerateStableId();
    if (!AssignPersistentEntityId(doorScene, doorEntity, doorId, error) ||
        !AssignPersistentEntityId(doorScene, playerEntity, playerId, error))
    {
        return Fail("assign real Action fixture identity: " + error);
    }

    const auto metadata = EvaluateScriptMetadata(
        root.generic_u8string(), doorSourcePath);
    if (!metadata.succeeded)
        return Fail("real Sliding Door metadata did not evaluate");

    ScriptDocument doorDocument = CreateScriptDocument(
        projectId,
        GenerateStableId(),
        "Content/Scenes/S7Door.wiscene",
        "s7-stock-actions-tests");
    ScriptSourceBinding doorBinding;
    doorBinding.sourceId = GenerateStableId();
    doorBinding.sourcePath = doorSourcePath;
    doorBinding.presentation = ScriptPresentation::Action;
    doorBinding.apiVersion = RuntimeScriptRuntime::ApiVersion;
    doorBinding.provenance.kind = ScriptProvenanceKind::Project;
    doorBinding.provenance.contentHash = "s7-real-door-fixture";
    ScriptAttachment doorAttachment = CreateScriptAttachment(
        ScriptScope::Entity, doorId, doorBinding);
    if (!ApplyScriptMetadataDefaults(
            metadata.descriptor, doorAttachment, error) ||
        !AddScriptAttachment(doorDocument, std::move(doorAttachment), error))
    {
        return Fail("attach real Sliding Door Action: " + error);
    }

    RuntimePlayerState player;
    player.entity = playerEntity;
    GameplayInputFrame doorInput;
    doorInput.interactPressed = true;
    runtime.SetGameplayState(&player, &doorInput);
    if (!runtime.StartScene(
            doorDocument, doorScene, root.generic_u8string(), error))
    {
        return Fail("start real Sliding Door Action: " + error);
    }
    runtime.Update(0.25f);
    const auto* movedDoor = doorScene.transforms.GetComponent(doorEntity);
    if (runtime.CurrentPrompt() != "Press E to open / close")
        return Fail("real Sliding Door did not show its nearby E prompt");
    if (movedDoor == nullptr || movedDoor->translation_local.y <= 0.0f)
        return Fail("real Sliding Door did not move when E was pressed nearby");
    if (runtime.DisabledInstanceCount() != 0 || !runtime.Diagnostics().empty())
        return Fail("real Sliding Door Action produced a Runtime diagnostic");

    runtime.StopScene();
    fs::remove_all(root, ec);
    std::cout << "S7 generic stock Action runtime seams passed.\n";
    return 0;
}
