#include "renegade/bridge/ScreenComposition.h"
#include <iostream>
using namespace renegade::bridge;
int main()
{
    int failures=0;
    const auto check=[&](bool valid,const char* message){
        if(!valid){++failures;std::cerr<<message<<'\n';}
    };
    ScreenCompositionCoordinator composition;
    std::string error;
    auto hud=DefaultScreenCompositionPolicy(ScreenCompositionLayer::Hud);
    auto overlay=DefaultScreenCompositionPolicy(ScreenCompositionLayer::Overlay);
    auto modal=DefaultScreenCompositionPolicy(ScreenCompositionLayer::Modal);
    check(composition.Activate({1,ScreenCompositionLayer::Hud,hud,false},error),"HUD activation failed");
    auto state=composition.Resolve();
    check(!state.gameplayInputBlocked && !state.gameplayPauseRequested &&
        state.uiInputOwners.empty() && state.keyboardFocusOwner==0,"Passive HUD captured gameplay");
    check(composition.Activate({2,ScreenCompositionLayer::Overlay,overlay,true},error),"Overlay activation failed");
    state=composition.Resolve();
    check(state.keyboardFocusOwner==2 && !state.gameplayPauseRequested &&
        composition.CanActivateControl(2),"Controls-only overlay policy failed");
    check(composition.Activate({3,ScreenCompositionLayer::Modal,modal,true},error),"Modal activation failed");
    state=composition.Resolve();
    check(state.gameplayPauseRequested && state.gameplayInputBlocked &&
        state.cursor==ScreenCursorPolicy::Show && state.keyboardFocusOwner==3 &&
        state.uiInputOwners.size()==1 && !composition.CanActivateControl(2),
        "Modal did not exclusively capture input and own its pause request");
    check(composition.Activate({4,ScreenCompositionLayer::Modal,modal,true},error),"Nested modal activation failed");
    check(composition.BeginExit(4,error),"Modal exit failed");
    state=composition.Resolve();
    check(state.uiInputOwners.empty() && state.keyboardFocusOwner==0 &&
        state.gameplayPauseRequested && state.gameplayInputBlocked,
        "An exiting modal leaked input before its visual lifetime ended");
    check(composition.Remove(4,error) && composition.Resolve().keyboardFocusOwner==3 &&
        composition.Resolve().gameplayPauseRequested,"Closing top modal lost lower modal focus/pause");
    check(composition.Remove(3,error),"Modal removal failed");
    state=composition.Resolve();
    check(!state.gameplayPauseRequested && !state.gameplayInputBlocked &&
        state.keyboardFocusOwner==2 && state.cursor==ScreenCursorPolicy::Inherit,
        "Overlay focus/gameplay did not restore after modal removal");
    const auto revision=composition.Revision();
    modal.blockGameplayInput=false;
    check(!composition.Update(2,modal,true,error) && composition.Revision()==revision &&
        composition.Resolve().keyboardFocusOwner==2,"Invalid policy partially changed an active instance");
    check(!composition.Activate({3,ScreenCompositionLayer::Modal,
        DefaultScreenCompositionPolicy(ScreenCompositionLayer::Modal),true},error),
        "A removed instance identity was reused for late input");
    composition.Clear();
    check(!composition.Resolve().gameplayPauseRequested &&
        !composition.Activate({1,ScreenCompositionLayer::Hud,hud,false},error),
        "Clear retained a pause request or reused an old instance identity");
    ScreenCompositionCoordinator layering;
    hud.order=1024;
    modal=DefaultScreenCompositionPolicy(ScreenCompositionLayer::Modal);modal.order=-1024;
    check(layering.Activate({1,ScreenCompositionLayer::Modal,modal,true},error) &&
        layering.Activate({2,ScreenCompositionLayer::Hud,hud,false},error),"Layer fixtures failed");
    state=layering.Resolve();
    check(state.drawOrder.size()==2 && state.drawOrder[0]==2 && state.drawOrder[1]==1 &&
        state.keyboardFocusOwner==1,"HUD order escaped its composition layer");
    ScreenCompositionCoordinator bounded;
    for(std::uint64_t id=1;id<=32;++id)
        check(bounded.Activate({id,ScreenCompositionLayer::Hud,hud,false},error),"Bounded instance activation failed");
    const auto before=bounded.Revision();
    check(!bounded.Activate({33,ScreenCompositionLayer::Hud,hud,false},error) &&
        bounded.Instances().size()==32 && bounded.Revision()==before,"Instance budget was not atomic");
    if(failures)return 1;
    std::cout<<"PASS: composition layering, modal focus/input, lifetime pause, exit shields and bounded identities\n";
}
