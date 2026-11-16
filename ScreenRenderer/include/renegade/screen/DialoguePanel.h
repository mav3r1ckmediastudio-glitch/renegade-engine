#pragma once
#include "renegade/bridge/DialogueService.h"
#include "renegade/screen/ScreenRenderer.h"
namespace renegade::screen
{
// Shared Screen renderer and input surfaces for editor preview and game.
class DialoguePanel
{
  public:
    ~DialoguePanel() { StopVoice(); }
    bool Show(const bridge::DialogueView &, const std::string &project, const std::string &root, wi::RenderPath2D &,
              bridge::ScreenRect viewport, std::string &error);
    void Hide(wi::RenderPath2D &path)
    {
        StopVoice();
        renderer_.Reset(path);
        ids_.clear();
        pending_.clear();
    }
    std::string TakeAction()
    {
        auto r = pending_;
        pending_.clear();
        if (r == "@next")
        {
            ++page_;
            return "@refresh";
        }
        if (r == "@previous")
        {
            if (page_)
                --page_;
            return "@refresh";
        }
        return r;
    }
    void EmbedInWindow(wi::RenderPath2D &path, wi::gui::Window &parent)
    {
        renderer_.EmbedInWindow(path, parent);
    }
    void SetAccessibilitySettings(bridge::ScreenAccessibilitySettings settings)
    {
        renderer_.SetAccessibilitySettings(settings);
    }
    void StopVoice();
    void Navigate(int direction);
    void Activate();
    void Layout(wi::RenderPath2D &path, bridge::ScreenRect r)
    {
        renderer_.SetViewport(r);
        renderer_.ApplyLayout(path);
    }

  private:
    ScreenRenderer renderer_;
    std::vector<std::string> ids_, actions_;
    size_t focus_ = 0, page_ = 0;
    std::string pending_, voiceKey_;
    wi::Resource voiceResource_;
    wi::audio::SoundInstance voiceInstance_;
};
} // namespace renegade::screen