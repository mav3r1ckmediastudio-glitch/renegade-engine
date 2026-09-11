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

    inline constexpr const char* Phase7AnimationSectionId = "phase7.animation";
    inline constexpr const char* Phase7RigSectionId = "phase7.rig";
    inline constexpr const char* Phase7IkExpressionSectionId = "phase7.ik_expression";
    inline constexpr const char* Phase7TimelineSectionId = "phase7.timeline";
    inline constexpr const char* Phase7SpecialistSectionId = "phase7.specialist";

    void RegisterPhase7NativeInspector(
        StudioRenderPath& owner,
        wi::gui::Window& inspectorPanel,
        InspectorSectionRegistry& registry,
        std::function<void()> requestRefresh,
        std::function<void(std::string)> setStatus);

    // Called before each shared Inspector layout. It clears stale controls and
    // also lays out Gate 7E's Terrain-specific advanced controls because the
    // legacy Inspector intentionally exits early while the Terrain workspace
    // owns the right-hand panel.
    void PreparePhase7NativeInspector(StudioRenderPath& owner);
}
