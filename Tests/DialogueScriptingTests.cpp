#include "RuntimeDialogue.h"
#include "RuntimeScriptRuntime.h"
#include "renegade/bridge/IdentityService.h"
#include <filesystem>
#include <fstream>
#include <iostream>
using namespace renegade::bridge;
using namespace renegade::runtime;
#define CHECK(x) do { if (!(x)) { std::cerr << "Dialogue scripting line " << __LINE__ << ": " << #x << " " << error << '\n'; return 1; } } while (false)
int main()
{
    namespace fs = std::filesystem;
    const auto root = fs::temp_directory_path() / ("renegade-dialogue-lua-" + GenerateStableId());
    fs::create_directories(root / "Content/Scripts");
    const auto project = GenerateStableId(), asset = GenerateStableId();
    std::string error;
    auto document = MakeGuardDialogue(project, asset);
    // Author a genuine conditional outcome using the existing format.
    auto &gate = document.lines.front().replies[1];
    auto permitted = gate.outcomes.front();
    permitted.next = "permit";
    permitted.conditions = {{DialogueConditionKind::FlagEquals, "has_permit", 1}};
    permitted.actions.push_back({DialogueActionKind::Event, "open_gate", 1});
    gate.outcomes.insert(gate.outcomes.begin(), permitted);
    CHECK(SaveDialogueAsset(root.generic_u8string(), document, error));
    auto duplicate = document;
    duplicate.id = GenerateStableId();
    CHECK(SaveDialogueAsset(root.generic_u8string(), duplicate, error));
    auto unique = document;
    unique.id = GenerateStableId(); unique.name = "Quest greeting";
    CHECK(SaveDialogueAsset(root.generic_u8string(), unique, error));
    auto scene = wi::allocator::make_shared<wi::scene::Scene>();
    const auto npc = scene->Entity_CreateTransform("Guard");
    const StableId npcId = GenerateStableId();
    CHECK(AssignPersistentEntityId(*scene, npc, npcId, error));
    AssignDialogueCommand assignment(*scene, npc, asset);
    CHECK(assignment.Execute());
    wi::RenderPath2D path;
    RuntimeDialogue dialogue;
    RuntimeScriptRuntime scripts;
    dialogue.PrepareScene(*scene, 1, root.generic_u8string(), project, path, wi::ecs::INVALID_ENTITY);
    dialogue.SetLifecycleSink([&](const std::string &name, const std::string &id, const std::string &sender, std::string &e) {
        GameplayEvent event; event.name = name; event.payload = id; event.senderEntityId = sender;
        return scripts.EnqueueGameplayEvent(std::move(event), e);
    });
    scripts.SetDialogueApi(dialogue.ScriptApi(*scene, path));
    dialogue.SetEventSink([&](const std::vector<DialogueAction> &actions, std::string &e) {
        if (actions.size() != 1) return false;
        GameplayEvent event; event.name = "dialogue.action";
        event.payload = actions[0].key + ":" + std::to_string(actions[0].value);
        return scripts.EnqueueGameplayEvent(std::move(event), e);
    });
    const std::string source = R"lua(
local d = renegade.dialogue
return {
 on_start=function(ctx)
  assert(d and d.contract_version==1 and not d.is_active())
  assert(d.get_flag('has_permit')==false and d.get_value('quest.stage')==0)
  assert(d.set_flag('has_permit',true))
  assert(d.set_value('quest.stage',3) and d.get_value('quest.stage')==3)
  assert(d.set_value('negative',-2147483648))
  assert(d.set_value('maximum',2147483647))
  assert(d.set_flag('has_permit',1)==false)
  assert(d.set_value('quest.stage',3.5)==false)
  assert(d.set_value('quest.stage',2147483648)==false)
  assert(d.set_value('quest.stage','4')==false)
  assert(d.set_flag('../escape',true)==false)
  assert(d.set_flag('nul'..string.char(0)..'key',true)==false)
  assert(d.get_value('bad key')==nil and d.get_value('quest.stage')==3)
  assert(d.start('missing')==false and not d.is_active())
  assert(d.start('Gate Guard')==false and not d.is_active())
  assert(d.start('Quest greeting','missing-line')==false)
  assert(d.start({})==false)
  assert(d.start(ctx.entity))
  assert(d.is_active() and d.start(ctx.entity)==false)
 end,
 on_event=function(ctx,event)
  if event.name=='dialogue.started' then
   assert(type(event.payload)=='string' and #event.payload>0)
   assert(d.set_value('starts',d.get_value('starts')+1))
   if d.get_value('starts')==1 then assert(renegade.entity.is_valid(event.sender)) end
  elseif event.name=='dialogue.finished' then
   assert(not d.is_active())
   assert(d.set_value('finishes',d.get_value('finishes')+1))
  elseif event.name=='dialogue.closed' then
   assert(not d.is_active())
   assert(d.set_value('closes',d.get_value('closes')+1))
  elseif event.name=='dialogue.action' then
   assert(event.payload=='open_gate:1')
   assert(d.set_flag('gate_opened',true))
  elseif event.name=='qa.deleted' then
   local ok,err=d.start(ctx.entity)
   assert(ok==false and type(err)=='string' and not d.is_active())
   assert(d.set_flag('deleted_checked',true))
  elseif event.name=='qa.capacity' then
   assert(d.set_value('new_key',1)==false)
   assert(d.set_value('quest.stage',5))
  end
 end
}
)lua";
    { std::ofstream out(root / "Content/Scripts/guard.lua"); out << source; }
    auto scriptDocument = CreateScriptDocument(project, GenerateStableId(), "Content/Scenes/Dialogue.wiscene", "dialogue-tests");
    ScriptSourceBinding binding;
    binding.sourceId = GenerateStableId(); binding.sourcePath = "Content/Scripts/guard.lua";
    binding.apiVersion = RuntimeScriptRuntime::ApiVersion;
    binding.provenance.kind = ScriptProvenanceKind::Project;
    binding.provenance.contentHash = "dialogue-tests";
    CHECK(AddScriptAttachment(scriptDocument, CreateScriptAttachment(ScriptScope::Entity, npcId, binding), error));
    CHECK(scripts.StartScene(scriptDocument, *scene, root.generic_u8string(), error));
    auto healthy = [&] {
        for (const auto &diagnostic : scripts.Diagnostics()) std::cerr << diagnostic.message << '\n';
        return scripts.Diagnostics().empty() && scripts.DisabledInstanceCount() == 0;
    };
    CHECK(healthy());
    CHECK(dialogue.Active());
    // A first Update's scene preparation must not close on_start's session.
    dialogue.PrepareScene(*scene, 1, root.generic_u8string(), project, path, wi::ecs::INVALID_ENTITY);
    CHECK(dialogue.Active());
    scripts.Update(0.016f);
    CHECK(healthy() && dialogue.Memory().values.at("starts") == 1);
    CHECK(dialogue.Choose("gate", error));
    CHECK(dialogue.View().text == "Everything seems in order.");
    CHECK(dialogue.Continue(error));
    scripts.Update(0.016f);
    CHECK(healthy() && dialogue.Memory().values.at("finishes") == 1 && dialogue.Memory().values.at("gate_opened") == 1);
    auto api = dialogue.ScriptApi(*scene, path);
    CHECK(api.setValue("has_permit", 0, error));
    CHECK(api.start(npc, {}, {}, error));
    scripts.Update(0.016f);
    CHECK(dialogue.Choose("gate", error));
    CHECK(dialogue.View().text == "Come back with a permit.");
    CHECK(dialogue.Continue(error) && dialogue.Active()); // refusal returns to greeting
    CHECK(dialogue.Choose("bye", error) && !dialogue.Active());
    scripts.Update(0.016f);
    CHECK(healthy() && dialogue.Memory().values.at("finishes") == 2);
    CHECK(api.start(wi::ecs::INVALID_ENTITY, "Quest greeting", "captain", error));
    CHECK(dialogue.View().text == "Speak to the captain in the courtyard.");
    scripts.Update(0.016f);
    CHECK(api.stop(error) && api.stop(error)); // exactly one close event
    scripts.Update(0.016f);
    CHECK(healthy() && dialogue.Memory().values.at("closes") == 1);
    CHECK(api.start(wi::ecs::INVALID_ENTITY, asset, {}, error));
    scripts.Update(0.016f);
    CHECK(api.stop(error)); scripts.Update(0.016f);
    scene->Entity_Remove(npc);
    GameplayEvent event; event.name = "qa.deleted";
    CHECK(scripts.EnqueueGameplayEvent(event, error)); scripts.Update(0.016f);
    CHECK(healthy() && dialogue.Memory().values.at("deleted_checked") == 1);
    // Memory is the same saved-state codec used by ordinary NPC interaction.
    DialogueMemory restored;
    CHECK(DecodeDialogueMemory(EncodeDialogueMemory(dialogue.Memory()), restored, error));
    CHECK(restored.values == dialogue.Memory().values);
    while (dialogue.Memory().values.size() < 16384)
        dialogue.Memory().values["capacity_" + std::to_string(dialogue.Memory().values.size())] = 0;
    event.name = "qa.capacity";
    CHECK(scripts.EnqueueGameplayEvent(event, error)); scripts.Update(0.016f);
    CHECK(healthy() && dialogue.Memory().values.at("quest.stage") == 5);
    scripts.StopScene();
    dialogue.SetLifecycleSink({});
    scripts.SetDialogueApi({});
    fs::remove_all(root);
    std::cout << "Dialogue scripting: governed Lua, real saved assets, conditional outcomes, lifecycle, liveness and saved state passed.\n";
    return 0;
}
