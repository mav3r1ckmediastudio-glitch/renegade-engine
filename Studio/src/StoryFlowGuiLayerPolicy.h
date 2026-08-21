#pragma once

namespace renegade::studio
{
    // Wicked renders GUI widgets in reverse registration order. The full-screen
    // Story Flow workspace must therefore be registered after every lifecycle
    // control so it is painted first, behind those controls. The condition
    // editor is re-registered immediately before the workspace so it remains
    // above the canvas while Level/Screen controls remain frontmost.
    template <typename Gui, typename WorkspaceWidget, typename ConditionWidget>
    void PlaceStoryFlowWorkspaceBehindLifecycleControls(
        Gui& gui,
        WorkspaceWidget& workspace,
        ConditionWidget& conditionEditor)
    {
        gui.RemoveWidget(&conditionEditor);
        gui.RemoveWidget(&workspace);
        gui.AddWidget(&conditionEditor);
        gui.AddWidget(&workspace);
    }
}
