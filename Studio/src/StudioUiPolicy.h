#pragma once

namespace renegade::studio::ui
{
    constexpr float InspectorContentWidth(float panelWidth, float scrollbarWidth) noexcept
    {
        const float width = panelWidth - scrollbarWidth - 24.f;
        return width > 1.f ? width : 1.f;
    }

    struct SceneOverlayState
    {
        bool levelWorkspace = true;
        bool hub = false;
        bool loading = false; // Includes the visible Ready/Idle handoff cover.
        bool runtime = false;
        bool physics = false;
        bool toolWindow = false;

        constexpr bool Visible() const noexcept
        {
            return levelWorkspace && !hub && !loading && !runtime && !physics && !toolWindow;
        }
    };

    // The same state vocabulary is used by native buttons and custom menu rows.
    struct ButtonVisual
    {
        bool enabled = true;
        bool hovered = false;
        bool pressed = false;
        bool primary = false;
        bool selected = false;
        constexpr float Depression() const noexcept { return enabled && pressed ? 2.0f : 0.0f; }
    };

    class ReleaseAction
    {
    public:
        void Begin(int target) noexcept { target_ = target; }
        void Cancel() noexcept { target_ = -1; }
        int Target() const noexcept { return target_; }
        int Release(int hoveredTarget) noexcept
        {
            const int result = target_ >= 0 && target_ == hoveredTarget ? target_ : -1;
            Cancel();
            return result;
        }
    private:
        int target_ = -1;
    };
}
