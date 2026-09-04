#pragma once

#include <functional>
#include <string>

namespace wi::gui
{
    class Window;
}

namespace renegade::studio
{
    class InspectorSectionRegistry;
    class StudioRenderPath;

    inline constexpr const char* S4BActionSectionId = "action";
    inline constexpr const char* S4BScriptSectionId = "script";

    void RegisterS4BScriptAttachmentInspector(
        StudioRenderPath& owner,
        wi::gui::Window& inspectorPanel,
        InspectorSectionRegistry& registry,
        std::function<void()> requestRefresh,
        std::function<void(std::string)> setStatus);

    // Called before the shared registry lays out the current Inspector pass.
    // This hides stale row controls from the previous selection/expanded role;
    // providers then reveal only the controls that own the new layout.
    void PrepareS4BScriptAttachmentInspector(StudioRenderPath& owner);
}
