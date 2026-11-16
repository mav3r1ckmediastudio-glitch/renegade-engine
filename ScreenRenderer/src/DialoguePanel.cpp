#include "renegade/screen/DialoguePanel.h"
#include <WickedEngine.h>
#include "renegade/bridge/DialogueAssetService.h"
#include "renegade/bridge/AudioService.h"
#include <algorithm>
namespace renegade::screen
{
bool DialoguePanel::Show(const bridge::DialogueView &view, const std::string &project, const std::string &root,
                         wi::RenderPath2D &path, bridge::ScreenRect viewport, std::string &error)
{
    using namespace bridge;
    renderer_.Reset(path);
    ids_.clear();
    actions_.clear();
    focus_ = 0;
    if (!view.active)
    {
        StopVoice();
        return true;
    }
    ScreenDocument d;
    d.envelope =
        CreateDocumentEnvelope(project, "runtime-screen", "Content/Dialogue/Preview.renegade-screen", "dialogue-v1");
    d.designWidth = 1200;
    d.designHeight = 220;
    d.usage = ScreenUsage::Overlay;
    d.enabledInGame = false;
    auto add = [&](ScreenWidgetKind kind, std::string text, ScreenRect rect, std::string action = "") {
        ScreenWidget w;
        w.id = GenerateStableId();
        w.name = action.empty() ? (text.empty() ? "Dialogue panel" : text) : action;
        w.kind = kind;
        w.text = std::move(text);
        w.rect = rect;
        w.style = MakeScreenWidgetStyleTemplate(kind, rect.height);
        w.style.text.fontSize = view.theme.textSize;
        w.style.text.wrap = true;
        const auto accent = ScreenColor{std::uint8_t(view.theme.accent >> 16), std::uint8_t(view.theme.accent >> 8),
                                        std::uint8_t(view.theme.accent), 255};
        w.style.normal.background =
            kind == ScreenWidgetKind::Button ? ScreenColor{26, 31, 35, 255} : ScreenColor{8, 12, 16, 255};
        if (kind == ScreenWidgetKind::Text)
        {
            w.style.text.horizontalAlignment = ScreenHorizontalAlignment::Left;
            w.style.text.verticalAlignment = ScreenVerticalAlignment::Top;
        }
        w.style.normal.foreground = {240, 243, 242, 255};
        w.style.hover.background = w.style.focused.background = {44, 35, 29, 255};
        w.style.hover.foreground = w.style.focused.foreground = {244, 244, 244, 255};
        w.style.borderColor = accent;
        w.style.borderWidth = kind == ScreenWidgetKind::Button ? 1.f : 0.f;
        w.style.cornerRadius = 5;
        w.style.opacity = view.theme.panelOpacity;
        w.actionId = action.empty() ? "" : "dialogue.action." + std::to_string(ids_.size());
        if (!action.empty())
        {
            d.actions.push_back({w.actionId});
            d.focusOrder.push_back(w.id);
            ids_.push_back(w.id);
            actions_.push_back(action);
        }
        d.widgets.push_back(w);
    };
    add(ScreenWidgetKind::Image, "", {0, 0, 1200, 220});
    add(ScreenWidgetKind::Text, view.speaker, {26, 18, 520, 36});
    d.widgets.back().style.normal.foreground = {std::uint8_t(view.theme.accent >> 16), std::uint8_t(view.theme.accent >> 8), std::uint8_t(view.theme.accent), 255};
    add(ScreenWidgetKind::Text, view.text, {26, 62, 520, 144});
    // Four visible replies per page; all remaining replies remain reachable.
    const auto pages = (view.replies.size() + 3) / 4;
    page_ = pages ? std::min(page_, pages - 1) : 0;
    for (size_t i = page_ * 4; i < std::min(view.replies.size(), page_ * 4 + 4); ++i)
        add(ScreenWidgetKind::Button, view.replies[i].second, {580, 18 + float(i % 4) * 40, 594, 36},
            view.replies[i].first);
    if (view.replies.empty())
        add(ScreenWidgetKind::Button, "Continue", {580, 18, 594, 36}, "@continue");
    if (pages > 1)
    {
        add(ScreenWidgetKind::Button, "Previous", {580, 184, 186, 28}, "@previous");
        add(ScreenWidgetKind::Button, "Next", {780, 184, 186, 28}, "@next");
    }
    add(ScreenWidgetKind::Button, "Close", {980, 184, 194, 28}, "@close");
    renderer_.SetViewport(viewport);
    if (!renderer_.Load(
            d, root, path,
            [this](const std::string &id) {
                auto it = std::find(ids_.begin(), ids_.end(), id);
                if (it != ids_.end())
                    pending_ = actions_[size_t(it - ids_.begin())];
            },
            error))
        return false;
    renderer_.SetViewport(viewport);
    renderer_.ApplyLayout(path);
    if (!ids_.empty())
        renderer_.SetFocusedWidget(ids_[0]);
    // Rebuilding for focus/pagination must not restart a line's voice. A new
    // session visit (including a loop to the same line) has its own voice key.
    const auto nextVoiceKey = root + "/" + view.voiceKey + "/" + view.voice;
    if (voiceKey_ != nextVoiceKey)
    {
        StopVoice();
        voiceKey_ = nextVoiceKey;
        if (!view.voice.empty())
        {
            std::string filename, voiceError;
            if (bridge::ResolveDialogueVoicePath(root, view.voice, filename, voiceError) &&
                bridge::ValidateAudioAssetForWicked(filename, voiceError))
            {
                voiceResource_ = wi::resourcemanager::Load(filename);
                voiceInstance_.type = wi::audio::SUBMIX_TYPE_USER1;
                voiceInstance_.SetLooped(false);
                voiceInstance_.SetEnableReverb(false);
                if (voiceResource_.IsValid() &&
                    wi::audio::CreateSoundInstance(&voiceResource_.GetSound(), &voiceInstance_))
                {
                    wi::audio::SetVolume(1.f, &voiceInstance_);
                    wi::audio::Play(&voiceInstance_);
                    wi::backlog::post("Dialogue voice: " + view.voice);
                }
                else voiceError = "Could not create dialogue voice playback.";
            }
            // An unavailable audio device or optional clip never blocks text.
            if (!voiceError.empty()) wi::backlog::post("Dialogue voice: " + voiceError);
        }
    }
    return true;
}
void DialoguePanel::StopVoice()
{
    if (voiceInstance_.IsValid()) wi::audio::Stop(&voiceInstance_);
    voiceInstance_ = {};
    voiceResource_ = {};
    voiceKey_.clear();
}
void DialoguePanel::Navigate(int direction)
{
    if (ids_.empty())
        return;
    focus_ = (focus_ + ids_.size() + (direction < 0 ? -1 : 1)) % ids_.size();
    renderer_.SetFocusedWidget(ids_[focus_]);
}
void DialoguePanel::Activate()
{
    if (!actions_.empty())
        pending_ = actions_[focus_];
}
} // namespace renegade::screen