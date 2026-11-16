#include "../Studio/src/StudioUiPolicy.h"
#include <cstdlib>
#include <iostream>

using namespace renegade::studio::ui;
static void Check(bool condition, const char* message)
{
    if (!condition) { std::cerr << message << '\n'; std::exit(1); }
}
int main()
{
    // Every blocking surface must independently suppress world markers,
    // including a loading cover retained after background work completes.
    for (unsigned mask = 0; mask < 64; ++mask)
    {
        SceneOverlayState surface;
        surface.levelWorkspace = !(mask & 1);
        surface.hub = (mask & 2) != 0;
        surface.loading = (mask & 4) != 0;
        surface.runtime = (mask & 8) != 0;
        surface.physics = (mask & 16) != 0;
        surface.toolWindow = (mask & 32) != 0;
        Check(surface.Visible() == (mask == 0), "World overlay escaped a blocking surface");
    }
    Check(InspectorContentWidth(280.f,20.f)==236.f, "Narrow inspector forgot its scrollbar column");
    Check(InspectorContentWidth(360.f,20.f)==316.f, "Normal inspector content width is wrong");
    Check(InspectorContentWidth(20.f,20.f)>0.f, "Tiny width produced a negative control size");
    for (const float panel : {280.f,360.f,480.f})
        Check(12.f+InspectorContentWidth(panel,20.f)<=panel-20.f-12.f,
              "Control or numeric field intrudes into the scrollbar");
    ReleaseAction gesture;
    Check(gesture.Release(2) == -1, "Release without press invoked an action");
    gesture.Begin(2);
    Check(gesture.Target() == 2, "Held row lost its visual state");
    Check(gesture.Release(2) == 2, "Matching release did not invoke");
    Check(gesture.Release(2) == -1, "One press invoked twice");
    gesture.Begin(2);
    Check(gesture.Release(3) == -1, "Dragging onto a different row invoked it");
    gesture.Begin(2);
    Check(gesture.Release(-1) == -1, "Dragging outside invoked a row");
    gesture.Begin(2); gesture.Cancel();
    Check(gesture.Release(2) == -1, "Escape/focus loss left a pending action");
    gesture.Begin(32 + 11);
    Check(gesture.Release(32 + 11) == 43, "Last Add row cannot activate");
    ButtonVisual button;
    Check(button.Depression() == 0, "Idle button is depressed");
    button.pressed = true;
    Check(button.Depression() == 2, "Held button lacks tactile offset");
    button.enabled = false;
    Check(button.Depression() == 0, "Disabled button responds to press");
    std::cout << "Studio UI policy checks passed\n";
}
