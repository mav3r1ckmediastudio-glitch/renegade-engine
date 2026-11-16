#include "RuntimeDialogue.h"
#include <cmath>
#include <algorithm>
namespace renegade::runtime
{
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
bool RuntimeDialogue::Update(wi::scene::Scene &scene, std::uint64_t revision, const std::string &root,
                             const std::string &project, wi::RenderPath2D &path, wi::ecs::Entity player,
                             bridge::GameplayInputFrame input, bool allowed)
{
    using namespace bridge;
    if (revision_ != revision || project_ != project || root_ != root)
    {
        Close(path);
        bindings_.clear();
        if (project_ != project)
            memory_ = {};
        revision_ = revision;
        root_ = root;
        project_ = project;
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
                session_.Continue(error_);
            else if (action != "@refresh")
                session_.Choose(action, error_);
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
                if (session_.Start(closest->document, memory_, error_))
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
