#pragma once
#include "renegade/bridge/DialogueAssetService.h"
#include "renegade/bridge/GameplayInputService.h"
#include "renegade/screen/DialoguePanel.h"
#include "RuntimeDialogueScriptApi.h"
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
    void Close(wi::RenderPath2D &path);
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
    void SetInteractBinding(const bridge::GameplayActionBinding &);
    using LifecycleSink = std::function<bool(const std::string &, const std::string &, const std::string &, std::string &)>;
    void SetLifecycleSink(LifecycleSink sink) { lifecycle_ = std::move(sink); }
    // Scene discovery is performed once, before creator on_start callbacks.
    void PrepareScene(wi::scene::Scene &, std::uint64_t revision, const std::string &root,
                      const std::string &project, wi::RenderPath2D &, wi::ecs::Entity player);
    RuntimeDialogueScriptApi ScriptApi(wi::scene::Scene &, wi::RenderPath2D &);
    bool Choose(const std::string &, std::string &error);
    bool Continue(std::string &error);
    bridge::DialogueView View() const { return session_.View(); }
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
    std::vector<bridge::DialogueDocument> scriptAssets_;
    bool scriptAssetsLoaded_ = false;
    LifecycleSink lifecycle_;
    std::string activeAsset_, activeNpc_;
    wi::ecs::Entity player_ = wi::ecs::INVALID_ENTITY;
    bool Start(const bridge::DialogueDocument &, const std::string &entry, const std::string &npc, std::string &error);
    void Emit(const std::string &event);
    bool StartScript(wi::scene::Scene &, wi::ecs::Entity, const std::string &, const std::string &, std::string &);
    std::uint64_t revision_ = std::numeric_limits<std::uint64_t>::max();
    bridge::DialogueMemory memory_;
    bridge::DialogueSession session_;
    screen::DialoguePanel panel_;
    std::string root_, project_, prompt_, error_, interactKey_ = "E";
    bool refresh_ = false;
};
} // namespace renegade::runtime
