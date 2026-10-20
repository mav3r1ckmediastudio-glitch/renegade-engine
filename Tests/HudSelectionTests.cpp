#include "renegade/bridge/HudSelection.h"
#include "renegade/bridge/HudContextEventRules.h"
#include "../Runtime/src/RuntimeUiComposition.h"
#include <iostream>
#include <set>
using namespace renegade::bridge;
int main()
{
    int failures=0;std::string error;
    const auto check=[&](bool ok,const char* message){if(!ok){++failures;std::cerr<<message<<": "<<error<<'\n';}};
    HudReference project{GenerateStableId(),"Content/UI/Project.renegade-screen"};
    HudReference level{GenerateStableId(),"Content/UI/Level.renegade-screen"};
    HudReference vehicle{GenerateStableId(),"Content/UI/Vehicle.renegade-screen"};
    HudReference turret{GenerateStableId(),"Content/UI/Turret.renegade-screen"};
    std::set<StableId> enabled{project.documentId,level.documentId,vehicle.documentId,turret.documentId};
    const auto eligible=[&](const HudReference& ref){return enabled.count(ref.documentId)!=0;};
    HudSelectionController hud;
    check(hud.EnterLevel(1,project,{},error),"Default HUD Level entry failed");
    check(hud.Resolve(eligible)->documentId==project.documentId,"Level did not inherit project HUD");
    check(hud.PushContext(1,1,"enter_vehicle",vehicle,error) && hud.Resolve(eligible)->documentId==vehicle.documentId,"Enter vehicle did not switch HUD");
    check(hud.PushContext(1,2,"enter_turret",turret,error) && hud.Resolve(eligible)->documentId==turret.documentId,"Nested context did not win");
    check(hud.PopContext(1,2,error) && hud.Resolve(eligible)->documentId==vehicle.documentId,"Nested exit did not restore prior eligible HUD");
    enabled.erase(vehicle.documentId);
    check(hud.Resolve(eligible)->documentId==project.documentId,"Disabled contextual HUD did not fall back");
    check(hud.PopContext(1,1,error) && hud.Resolve(eligible)->documentId==project.documentId,"Vehicle exit did not restore base HUD");
    check(hud.EnterLevel(2,project,{LevelHudMode::Override,level},error) && hud.Resolve(eligible)->documentId==level.documentId,"Level override failed");
    const auto revision=hud.Revision();
    check(!hud.PopContext(1,1,error) && !hud.PushContext(1,3,"old_level",turret,error) &&
        hud.Revision()==revision && hud.Resolve(eligible)->documentId==level.documentId,"Old Level event changed new HUD");
    enabled.erase(level.documentId);
    check(hud.Resolve(eligible)->documentId==project.documentId,"Ineligible Level HUD did not fall back to project");
    check(hud.EnterLevel(3,project,{LevelHudMode::None,{}},error) && !hud.Resolve(eligible),"Explicit no-HUD Level inherited project");
    check(hud.PushContext(3,3,"enter_vehicle",turret,error) && hud.Resolve(eligible)->documentId==turret.documentId &&
        hud.PopContext(3,3,error) && !hud.Resolve(eligible),"Context exit lost explicit no-HUD base");
    auto invalid=project;invalid.pathHint="Content/UI/../External.renegade-screen";
    check(!hud.EnterLevel(4,invalid,{},error) && hud.LevelSession()==3,"Unsafe path partially changed Level");
    check(!hud.EnterLevel(4,project,{LevelHudMode::InheritProject,level},error) && hud.LevelSession()==3,"Invalid override mode partially changed Level");
    hud.LeaveLevel();check(!hud.Resolve(eligible) && !hud.PushContext(3,4,"late_exit",turret,error),"Leaving Level retained HUD/context eligibility");
    check(!hud.EnterLevel(3,project,{},error) && hud.EnterLevel(4,project,{},error),"Old Level lifetime was reused");
    // UI-022: the approved project/Level/context selection must change only
    // the HUD composition instance, never the governing Level or a primary
    // Screen. Nested exit restores the preceding eligible HUD in order.
    {
        HudSelectionController selection;
        renegade::runtime::RuntimeUiComposition presentation;
        ScreenDocument primary;
        primary.usage=ScreenUsage::Overlay;
        check(presentation.ReplacePrimary(primary,false,error),
            "Context fixture could not activate a primary Screen");
        const auto primaryIdentity=presentation.PrimaryIdentity();
        ScreenDocument hudDocument;
        hudDocument.usage=ScreenUsage::Hud;
        const auto attach=[&](const HudReference& expected)
        {
            const auto selected=selection.Resolve(eligible);
            if(!selected||selected->documentId!=expected.documentId)
            {check(false,"Context resolved a different or absent eligible HUD");return false;}
            return presentation.ReplaceHud(hudDocument,false,error);
        };
        enabled.insert(vehicle.documentId);
        enabled.insert(level.documentId);
        check(selection.EnterLevel(1,project,{LevelHudMode::Override,level},error),
            "Context fixture Level entry failed");
        check(attach(level),"Context fixture Level HUD could not attach");
        const auto levelHudInstance=presentation.HudIdentity();
        check(selection.PushContext(1,1,"enter_vehicle",vehicle,error),
            "Context fixture vehicle event was rejected");
        check(attach(vehicle),"Context fixture vehicle HUD could not attach");
        const auto vehicleHudInstance=presentation.HudIdentity();
        check(vehicleHudInstance>levelHudInstance &&
            presentation.PrimaryIdentity()==primaryIdentity &&
            selection.LevelSession()==1,
            "Context switch reloaded Level, changed primary identity or reused HUD identity");
        check(selection.PushContext(1,2,"enter_turret",turret,error),
            "Nested HUD context event was rejected");
        check(attach(turret),"Context fixture turret HUD could not attach");
        const auto turretHudInstance=presentation.HudIdentity();
        check(turretHudInstance>vehicleHudInstance,
            "Nested HUD context failed monotonic lifetime");
        check(selection.PopContext(1,2,error) && attach(vehicle),
            "Nested context exit did not restore vehicle HUD");
        enabled.erase(vehicle.documentId);
        check(attach(level),"Ineligible vehicle HUD did not fall back to Level override");
        check(selection.PopContext(1,1,error)&&attach(level),
            "Vehicle exit failed to restore previous eligible Level HUD");
        check(presentation.PrimaryIdentity()==primaryIdentity &&
            selection.LevelSession()==1 && presentation.IsHudVisible() &&
            !presentation.Resolve().gameplayPauseRequested,
            "Context restoration changed Level/primary instance or leaked pause ownership");
        const auto beforeOldEvent=selection.Revision();
        check(!selection.PopContext(9,2,error) &&
            selection.Revision()==beforeOldEvent &&
            presentation.PrimaryIdentity()==primaryIdentity,
            "Stale context exit changed current Screen ownership");
    }
    // Accepted gameplay events select HUD contexts independently of Flow,
    // with bounded exact-match pairs and monotonic Level/instance lifetimes.
    {
        enabled.insert(vehicle.documentId);
        enabled.insert(level.documentId);
        HudSelectionController eventSelection;
        HudContextEventRouter events;
        const std::vector<HudContextEventRule> rules{
            {"vehicle.enter","vehicle.exit",vehicle},
            {"turret.enter","turret.exit",turret}};
        check(events.Configure(rules,error)&&events.RuleCount()==2,
            "HUD event rule configuration rejected valid event pairs");
        check(eventSelection.EnterLevel(13,project,{},error),
            "Governed HUD event Level failed to enter");
        events.BeginLevel(13);
        bool matched=false;
        const auto send=[&](const char* event)
        {
            return events.Handle(GameplayEvent{0,event},13,eventSelection,matched,error);
        };
        check(send("not_hud_event")&&!matched &&
            eventSelection.Resolve(eligible)->documentId==project.documentId,
            "Unrelated gameplay event changed HUD selection");
        check(send("vehicle.enter")&&matched &&
            eventSelection.Resolve(eligible)->documentId==vehicle.documentId,
            "Governed enter_vehicle event did not select vehicle HUD");
        const auto revision=eventSelection.Revision();
        check(send("vehicle.enter")&&matched&&eventSelection.Revision()==revision,
            "Duplicate enter event created a second HUD context");
        check(send("turret.enter")&&matched &&
            eventSelection.Resolve(eligible)->documentId==turret.documentId,
            "Nested governed event did not select turret HUD");
        check(send("turret.exit")&&matched &&
            eventSelection.Resolve(eligible)->documentId==vehicle.documentId,
            "Governed turret exit did not restore vehicle HUD");
        check(send("vehicle.exit")&&matched &&
            eventSelection.Resolve(eligible)->documentId==project.documentId,
            "Governed vehicle exit did not restore project default HUD");
        check(send("vehicle.exit")&&matched &&
            eventSelection.Resolve(eligible)->documentId==project.documentId,
            "Repeated exit did not remain idempotent");
        check(!events.Handle(GameplayEvent{0,"vehicle.enter"},99,eventSelection,matched,error) &&
            eventSelection.Resolve(eligible)->documentId==project.documentId,
            "Old Level event switched current HUD context");
        auto invalid=rules;
        invalid[1].enterEvent="vehicle.exit";
        check(!events.Configure(invalid,error)&&events.RuleCount()==2,
            "Ambiguous HUD rule events partially replaced valid rules");
        check(eventSelection.EnterLevel(14,project,{},error),"HUD event replacement Level failed");
        events.BeginLevel(14);
        check(send("vehicle.enter")==false,
            "Stale event incorrectly accepted previous Level session");
        eventSelection.LeaveLevel();
        events.LeaveLevel();
    }
    if(failures)return 1;
    std::cout<<"PASS: project/Level HUD selection, nested context restore, disabled fallback and stale-event isolation\n";
}
