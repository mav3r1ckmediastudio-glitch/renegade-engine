#include "../Runtime/src/RuntimeUiComposition.h"
#include "../Runtime/src/RuntimeUiTimingWindow.h"
#include <iostream>
using namespace renegade;
int main()
{
    int failures=0;
    const auto check=[&](bool ok,const char* message){if(!ok){++failures;std::cerr<<message<<'\n';}};
    runtime::RuntimeUiTimingWindow meter;
    check(meter.WindowCount()==0&&meter.WindowAverageUs()==0,
        "UI timing meter did not initialize empty");
    for(std::size_t index=0;index<runtime::RuntimeUiTimingWindow::Capacity;++index)
        meter.Observe(1000);
    check(meter.WindowCount()==120&&meter.WindowAverageUs()==1000&&
        meter.PeakUs()==1000&&meter.OverBudgetInWindow()==0,
        "UI timing meter rolling window incorrect");
    meter.Observe(3200);
    check(meter.WindowCount()==120&&meter.WindowAverageUs()==1018&&
        meter.PeakUs()==3200&&meter.OverBudgetInWindow()==1,
        "UI timing meter did not track bounded advisory excursion");
    meter.Observe(60'000'001);
    check(meter.WindowCount()==120&&meter.LatestUs()==3200,
        "UI timing meter allowed an implausible outlier");
    meter.Reset();
    check(meter.WindowCount()==0&&meter.PeakUs()==0&&
        meter.LifetimeSamples()==0,"UI timing meter reset leaked another session");

    runtime::RuntimeUiComposition ui;bridge::ScreenDocument document;std::string error;
    document.usage=bridge::ScreenUsage::Menu;
    check(ui.ReplacePrimary(document,true,error),"Menu activation failed");
    auto state=ui.Resolve();
    check(state.gameplayPauseRequested && state.gameplayInputBlocked && ui.PrimaryAcceptsInput(),"Menu policy failed");
    const auto menu=ui.PrimaryIdentity();
    document.usage=bridge::ScreenUsage::Hud;
    check(ui.ReplacePrimary(document,false,error),"HUD activation failed");state=ui.Resolve();
    check(ui.PrimaryIdentity()>menu && !state.gameplayPauseRequested && !state.gameplayInputBlocked &&
        !ui.PrimaryAcceptsInput() && state.cursor==bridge::ScreenCursorPolicy::Inherit,"HUD captured gameplay or input");
    document.usage=bridge::ScreenUsage::Overlay;
    check(ui.ReplacePrimary(document,true,error),"Overlay activation failed");state=ui.Resolve();
    check(ui.PrimaryAcceptsInput() && !state.gameplayPauseRequested && !state.gameplayInputBlocked,"Overlay paused gameplay");
    document.usage=bridge::ScreenUsage::Modal;
    check(ui.ReplacePrimary(document,true,error),"Modal activation failed");
    check(ui.Resolve().gameplayPauseRequested,"Modal lost pause");
    document.enabledInGame=false;
    check(ui.ReplacePrimary(document,true,error) && ui.PrimaryIdentity()==0 &&
        !ui.Resolve().gameplayPauseRequested && ui.Resolve().drawOrder.empty(),"Disabled Screen retained ownership");
    document.enabledInGame=true;check(ui.ReplacePrimary(document,true,error),"Reactivation failed");
    const auto identity=ui.PrimaryIdentity();ui.ClearPrimary();ui.ClearPrimary();
    check(ui.Resolve().drawOrder.empty() && !ui.PrimaryAcceptsInput(),"Clear was not idempotent");
    check(ui.ReplacePrimary(document,true,error) && ui.PrimaryIdentity()>identity,"Reload reused identity");
    ui.ClearPrimary();
    bridge::ScreenDocument hud;hud.usage=bridge::ScreenUsage::Hud;
    check(ui.ReplaceHud(hud,false,error) && ui.IsHudVisible() && !ui.Resolve().gameplayPauseRequested,
        "Concurrent passive HUD captured gameplay");
    document.usage=bridge::ScreenUsage::Modal;
    check(ui.ReplacePrimary(document,true,error) && ui.Resolve().drawOrder.size()==2 &&
        ui.Resolve().drawOrder.front()==ui.HudIdentity() && ui.PrimaryAcceptsInput() && !ui.HudAcceptsInput(),
        "Modal did not compose above passive HUD with exclusive input");
    const auto hudIdentity=ui.HudIdentity();
    check(ui.SetHudUnderlayAllowed(false,error) && !ui.IsHudVisible() && ui.Resolve().drawOrder.size()==1 &&
        ui.Resolve().gameplayPauseRequested,"Hiding HUD lost modal pause ownership");
    check(ui.SetHudUnderlayAllowed(true,error) && ui.IsHudVisible() && ui.HudIdentity()>hudIdentity,
        "Restored HUD reused an old instance identity");
    ui.ClearPrimary();check(!ui.Resolve().gameplayPauseRequested && ui.Resolve().drawOrder.size()==1,
        "Closing modal retained pause or destroyed its HUD");
    auto pausedHud=bridge::DefaultScreenCompositionPolicy(bridge::ScreenCompositionLayer::Hud);
    pausedHud.pauseGameplay=true;pausedHud.blockGameplayInput=true;hud.composition=pausedHud;
    check(ui.ReplaceHud(hud,false,error),"Paused HUD setup failed");
    check(ui.ReplacePrimary(document,true,error),"Nested pause owner setup failed");
    ui.ClearPrimary();check(ui.Resolve().gameplayPauseRequested,"Closing one UI instance released another owner's pause");
    check(ui.SetHudUnderlayAllowed(false,error) && !ui.Resolve().gameplayPauseRequested,
        "Dormant HUD retained its pause request");
    check(ui.SetHudUnderlayAllowed(true,error) && ui.Resolve().gameplayPauseRequested,
        "Reactivated HUD lost its authored pause policy");
    ui.ClearHud();check(!ui.Resolve().gameplayPauseRequested && ui.Resolve().drawOrder.empty(),
        "HUD removal leaked ownership");
    if(failures)return 1;
    std::cout<<"PASS: runtime usage policies, disabled documents, identity and ownership release\n";
}
