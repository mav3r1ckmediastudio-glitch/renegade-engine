#pragma once
#include "renegade/bridge/DialogueAssetService.h"
#include "renegade/bridge/GameplayInputService.h"
#include "renegade/screen/DialoguePanel.h"
#include <limits>
namespace renegade::runtime
{
class RuntimeDialogue
{
  public:
    bool Active() const
    {
        return session_.Active();
    }
    const std::string &Prompt() const
    {
        return prompt_;
    }
    bridge::DialogueMemory &Memory()
    {
        return memory_;
    }
    void Close(wi::RenderPath2D &path)
    {
        session_.Close();
        panel_.Hide(path);
        prompt_.clear();
    }
    void Reset(wi::RenderPath2D &path)
    {
        Close(path);
        memory_ = {};
        revision_ = std::numeric_limits<std::uint64_t>::max();
    }
    void SetAccessibilitySettings(bridge::ScreenAccessibilitySettings settings)
    {
        panel_.SetAccessibilitySettings(settings);
    }
    void SetItemQuery(bridge::DialogueSession::ItemQuery q)
    {
        session_.SetItemQuery(std::move(q));
    }
    void SetEventSink(bridge::DialogueSession::EventSink sink)
    {
        session_.SetEventSink(std::move(sink));
    }
    // Returns true when this frame's gameplay input belongs to dialogue.
    bool Update(wi::scene::Scene &, std::uint64_t revision, const std::string &root, const std::string &project,
                wi::RenderPath2D &, wi::ecs::Entity player, bridge::GameplayInputFrame input, bool allowed);

  private:
    struct Binding
    {
        wi::ecs::Entity entity;
        bridge::DialogueDocument document;
    };
    std::vector<Binding> bindings_;
    std::uint64_t revision_ = std::numeric_limits<std::uint64_t>::max();
    bridge::DialogueMemory memory_;
    bridge::DialogueSession session_;
    screen::DialoguePanel panel_;
    std::string root_, project_, prompt_, error_;
    bool refresh_ = false;
};
} // namespace renegade::runtime
