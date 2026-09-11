#pragma once

#include <functional>
#include <string>
#include <utility>

#include "Phase7Gate7BHumanoidRetargetInspector.h"
#include "Phase7Gate7CCharacterControlsInspector.h"

namespace wi::gui
{
    class Window;
}

namespace renegade::studio
{
    class InspectorSectionRegistry;
    class StudioRenderPath;

    inline constexpr const char* Phase7AnimationSectionId = "animation";

    void RegisterPhase7Gate7AAnimationInspectorCore(
        StudioRenderPath& owner,
        wi::gui::Window& inspectorPanel,
        InspectorSectionRegistry& registry,
        std::function<void()> requestRefresh,
        std::function<void(std::string)> setStatus);

    void PreparePhase7Gate7AAnimationInspectorCore(StudioRenderPath& owner);

#ifdef RENEGADE_PHASE7A_IMPLEMENTATION
#define RegisterPhase7Gate7AAnimationInspector RegisterPhase7Gate7AAnimationInspectorCore
#define PreparePhase7Gate7AAnimationInspector PreparePhase7Gate7AAnimationInspectorCore
#else
    inline void RegisterPhase7Gate7AAnimationInspector(
        StudioRenderPath& owner,
        wi::gui::Window& inspectorPanel,
        InspectorSectionRegistry& registry,
        std::function<void()> requestRefresh,
        std::function<void(std::string)> setStatus)
    {
        RegisterPhase7Gate7BHumanoidRetargetInspector(
            owner, inspectorPanel, registry, requestRefresh, setStatus);
        RegisterPhase7Gate7CCharacterControlsInspector(
            owner, inspectorPanel, registry, requestRefresh, setStatus);
        RegisterPhase7Gate7AAnimationInspectorCore(
            owner,
            inspectorPanel,
            registry,
            std::move(requestRefresh),
            std::move(setStatus));
    }

    inline void PreparePhase7Gate7AAnimationInspector(StudioRenderPath& owner)
    {
        PreparePhase7Gate7BHumanoidRetargetInspector(owner);
        PreparePhase7Gate7CCharacterControlsInspector(owner);
        PreparePhase7Gate7AAnimationInspectorCore(owner);
    }
#endif
}
