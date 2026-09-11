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

    inline constexpr const char* Phase7CharacterControlsSectionId = "character_controls";

    void RegisterPhase7Gate7CCharacterControlsInspector(
        StudioRenderPath& owner,
        wi::gui::Window& inspectorPanel,
        InspectorSectionRegistry& registry,
        std::function<void()> requestRefresh,
        std::function<void(std::string)> setStatus);

    void PreparePhase7Gate7CCharacterControlsInspector(StudioRenderPath& owner);
}
