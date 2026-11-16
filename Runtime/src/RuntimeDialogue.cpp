#include "RuntimeDialogue.h"
#include <cmath>
#include <algorithm>
#include "renegade/bridge/IdentityService.h"
namespace renegade::runtime
{
void RuntimeDialogue::Emit(const std::string &event)
{
    if (lifecycle_ && !lifecycle_(event, activeAsset_, activeNpc_, error_))
        wi::backlog::post("Dialogue event: " + error_, wi::backlog::LogLevel::Error);
}
void RuntimeDialogue::Close(wi::RenderPath2D &path)
{
    if (Active()) Emit("dialogue.closed");
    session_.Close();
    panel_.Hide(path);
    prompt_.clear();
    activeAsset_.clear();
    activeNpc_.clear();
    refresh_ = false;
}
bool RuntimeDialogue::Start(const bridge::DialogueDocument &document, const std::string &entry,
                            const std::string &npc, std::string &error)
{
    if (Active()) { error = "A conversation is already active. Stop it before starting another."; return false; }
    auto selected = document;
    if (!entry.empty()) selected.entry = entry;
    if (!session_.Start(selected, memory_, error)) return false;
    activeAsset_ = document.id;
    activeNpc_ = npc;
    prompt_.clear();
    refresh_ = true;
    Emit("dialogue.started");
    error.clear();
    return true;
}
bool RuntimeDialogue::Choose(const std::string &reply, std::string &error)
{
    if (!session_.Choose(reply, error)) return false;
    refresh_ = true;
    if (!Active()) Emit("dialogue.finished");
    return true;
}
bool RuntimeDialogue::Continue(std::string &error)
{
    if (!session_.Continue(error)) return false;
    refresh_ = true;
    if (!Active()) Emit("dialogue.finished");
    return true;
}
bool RuntimeDialogue::StartScript(wi::scene::Scene &scene, wi::ecs::Entity npc, const std::string &key,
                                  const std::string &entry, std::string &error)
{
    if (Active()) { error = "A conversation is already active. Stop it before starting another."; return false; }
    if (npc != wi::ecs::INVALID_ENTITY)
    {
        if (npc == player_ || bridge::IsPlayerStart(scene, npc) || !scene.transforms.GetComponent(npc))
        { error = "Choose a live NPC with an assigned conversation."; return false; }
        const auto id = bridge::AssignedDialogue(scene, npc);
        if (id.empty()) { error = "This NPC has no assigned conversation."; return false; }
        for (const auto &binding : bindings_)
            if (binding.entity == npc && binding.document.id == id)
                return Start(binding.document, entry, bridge::PersistentEntityId(scene, npc), error);
        bridge::DialogueDocument document;
        if (!bridge::LoadDialogueAsset(root_, project_, id, document, error)) return false;
        return Start(document, entry, bridge::PersistentEntityId(scene, npc), error);
    }
    if (!scriptAssetsLoaded_)
    {
        std::string diagnostic;
        scriptAssets_ = bridge::ListDialogueAssets(root_, project_, diagnostic);
        scriptAssetsLoaded_ = true;
        if (!diagnostic.empty()) wi::backlog::post("Dialogue catalogue: " + diagnostic, wi::backlog::LogLevel::Warning);
    }
    // Stable IDs take precedence; duplicate human-readable names are rejected.
    for (const auto &document : scriptAssets_)
        if (document.id == key) return Start(document, entry, {}, error);
    const bridge::DialogueDocument *selected = nullptr;
    for (const auto &document : scriptAssets_)
        if (document.name == key)
        {
            if (selected) { error = "Several conversations have this name. Use the asset ID instead."; return false; }
            selected = &document;
        }
    if (!selected) { error = "No saved conversation matches this name or asset ID in the active project."; return false; }
    return Start(*selected, entry, {}, error);
}
RuntimeDialogueScriptApi RuntimeDialogue::ScriptApi(wi::scene::Scene &scene, wi::RenderPath2D &path)
{
    RuntimeDialogueScriptApi api;
    api.start = [this, &scene](wi::ecs::Entity npc, const std::string &key, const std::string &entry, std::string &error)
    { return StartScript(scene, npc, key, entry, error); };
    api.stop = [this, &path](std::string &error) { Close(path); error.clear(); return true; };
    api.isActive = [this] { return Active(); };
    api.getValue = [this](const std::string &key, int &value, std::string &error)
    {
        if (!bridge::IsDialogueMemoryKey(key)) { error = "Invalid dialogue memory key."; return false; }
        const auto found = memory_.values.find(key);
        value = found == memory_.values.end() ? 0 : found->second;
        error.clear(); return true;
    };
    api.setValue = [this](const std::string &key, int value, std::string &error)
    {
        if (!bridge::IsDialogueMemoryKey(key)) { error = "Invalid dialogue memory key."; return false; }
        if (memory_.values.find(key) == memory_.values.end() && memory_.values.size() >= 16384)
        { error = "Dialogue memory has reached its saved-state capacity."; return false; }
        memory_.values[key] = value;
        if (Active()) refresh_ = true;
        error.clear(); return true;
    };
    return api;
}
void RuntimeDialogue::SetInteractBinding(const bridge::GameplayActionBinding &binding)
{
    interactKey_ = binding.keyboard;
    if (interactKey_.empty())
    {
        if (!binding.mouse.empty()) interactKey_ = binding.mouse + " mouse button";
        else if (!binding.gamepad.empty()) interactKey_ = "Gamepad " + binding.gamepad;
        else interactKey_ = "your Interact control";
    }
    if (interactKey_ == "SPACE") interactKey_ = "Space";
    if (interactKey_ == "LSHIFT") interactKey_ = "Left Shift";
    if (interactKey_ == "ESCAPE") interactKey_ = "Esc";
    std::replace(interactKey_.begin(), interactKey_.end(), '_', ' ');
}
void RuntimeDialogue::PrepareScene(wi::scene::Scene &scene, std::uint64_t revision, const std::string &root,
                                   const std::string &project, wi::RenderPath2D &path, wi::ecs::Entity player)
{
    using namespace bridge;
    if (revision_ != revision || project_ != project || root_ != root)
    {
        Close(path);
        bindings_.clear();
        scriptAssets_.clear();
        scriptAssetsLoaded_ = false;
        player_ = player;
        if (project_ != project)
            memory_ = {};
        revision_ = revision;
        root_ = root;
        project_ = project;
        if (!panel_.ReloadDisplay(root,project,error_))
            wi::backlog::post("Dialogue display: " + error_ + " Using the built-in display.",wi::backlog::LogLevel::Warning);
        for (size_t i = 0; i < scene.metadatas.GetCount(); ++i)
        {
            auto entity = scene.metadatas.GetEntity(i);
            if (entity == player || IsPlayerStart(scene, entity)) continue;
            auto id = AssignedDialogue(scene, entity);
            if (id.empty())
                continue;
            DialogueDocument d;
            if (LoadDialogueAsset(root, project, id, d, error_))
                bindings_.push_back({entity, std::move(d)});
            else
                wi::backlog::post("Dialogue: " + error_, wi::backlog::LogLevel::Error);
        }
    }
}
bool RuntimeDialogue::Update(wi::scene::Scene &scene, std::uint64_t revision, const std::string &root,
                             const std::string &project, wi::RenderPath2D &path, wi::ecs::Entity player,
                             bridge::GameplayInputFrame input, bool allowed)
{
    PrepareScene(scene, revision, root, project, path, player);
    const bool owned = Active();
    prompt_.clear();
    if (owned)
    {
        if (!allowed)
        {
            Close(path);
            return true;
        }
        auto action = panel_.TakeAction();
        if (input.pausePressed || wi::input::Press(wi::input::KEYBOARD_BUTTON_ESCAPE) ||
            wi::input::Press(wi::input::GAMEPAD_BUTTON_XBOX_B))
            action = "@close";
        if (wi::input::Press(wi::input::KEYBOARD_BUTTON_DOWN) || wi::input::Press(wi::input::GAMEPAD_BUTTON_DOWN))
            panel_.Navigate(1);
        if (wi::input::Press(wi::input::KEYBOARD_BUTTON_UP) || wi::input::Press(wi::input::GAMEPAD_BUTTON_UP))
            panel_.Navigate(-1);
        if (wi::input::Press(wi::input::KEYBOARD_BUTTON_ENTER) || wi::input::Press(wi::input::GAMEPAD_BUTTON_XBOX_A))
            panel_.Activate();
        if (!action.empty())
        {
            if (action == "@close")
                Close(path);
            else if (action == "@continue")
                Continue(error_);
            else if (action != "@refresh")
                Choose(action, error_);
            refresh_ = true;
            wi::backlog::post("Dialogue action: " + action + " -> " + session_.View().voiceKey);
        }
    }
    else if (allowed)
    {
        const auto *pt = scene.transforms.GetComponent(player);
        Binding *closest = nullptr;
        float best = 9.f;
        if (pt)
        {
            auto p = pt->GetPosition();
            for (auto &binding : bindings_)
            {
                auto *t = scene.transforms.GetComponent(binding.entity);
                if (!t)
                    continue;
                auto q = t->GetPosition();
                float d = (q.x - p.x) * (q.x - p.x) + (q.y - p.y) * (q.y - p.y) + (q.z - p.z) * (q.z - p.z);
                if (d < best)
                {
                    best = d;
                    closest = &binding;
                }
            }
        }
        if (closest)
        {
            prompt_ = "Press " + interactKey_ + " to talk to " + closest->document.name;
            if (input.interactPressed || wi::input::Press(wi::input::GAMEPAD_BUTTON_XBOX_A))
            {
                if (Start(closest->document, {}, bridge::PersistentEntityId(scene, closest->entity), error_))
                    refresh_ = true;
                else
                    wi::backlog::post("Dialogue: " + error_);
            }
        }
    }
    if (refresh_)
    {
        refresh_ = false;
        if (Active())
        {
            if (!panel_.Show(session_.View(), project, root, path,
                             {24, path.GetLogicalHeight() - 300, path.GetLogicalWidth() - 48, 280}, error_))
            {
                wi::backlog::post("Dialogue UI: " + error_, wi::backlog::LogLevel::Error);
                Close(path);
            }
        }
        else
            panel_.Hide(path);
    }
    else if (Active())
        panel_.Layout(path, {24, path.GetLogicalHeight() - 300, path.GetLogicalWidth() - 48, 280});
    return owned || Active();
}
} // namespace renegade::runtime
