#include "RuntimeActions.h"
#include "RuntimeScreen.h"
#include "RuntimeUiComposition.h"
#include "RuntimeUiAccessibilityPreferences.h"

#include "renegade/bridge/ScreenService.h"
#include "renegade/bridge/ScreenWidgetPresets.h"
#include "renegade/bridge/InventoryScreenAdapter.h"

#include <iostream>
#include <string>
#include <filesystem>
#include <fstream>

namespace
{
    using namespace renegade;

    bridge::ScreenDocument MakeScreen()
    {
        bridge::ScreenDocument document;
        const bridge::StableId projectId = bridge::GenerateStableId();
        document.envelope = bridge::CreateDocumentEnvelope(
            projectId,
            bridge::RuntimeScreenDocumentType,
            "Content/UI/Main.renegade-screen",
            "Renegade LP03 Runtime screen tests");
        document.designWidth = 1280.0f;
        document.designHeight = 720.0f;
        document.actions = {
            {bridge::RuntimeScreenPlayAction},
            {bridge::RuntimeScreenQuitAction},
        };
        document.widgets = {
            {bridge::GenerateStableId(), bridge::ScreenWidgetKind::Image,
                "Background", {0, 0, 1280, 720}, true, false, {},
                "Content/UI/background.png", {}},
            {bridge::GenerateStableId(), bridge::ScreenWidgetKind::Text,
                "Title", {240, 120, 800, 100}, true, false,
                "RENEGADE", {}, {}},
            {bridge::GenerateStableId(), bridge::ScreenWidgetKind::Button,
                "Play", {440, 330, 400, 76}, true, true,
                "PLAY", {}, bridge::RuntimeScreenPlayAction},
            {bridge::GenerateStableId(), bridge::ScreenWidgetKind::Button,
                "Quit", {440, 440, 400, 76}, true, true,
                "QUIT", {}, bridge::RuntimeScreenQuitAction},
        };
        document.focusOrder = {
            document.widgets[2].id,
            document.widgets[3].id,
        };
        for (auto& widget : document.widgets)
        {
            widget.style = bridge::MakeScreenWidgetStyleTemplate(
                widget.kind, widget.rect.height);
        }
        return document;
    }

    int Fail(const char* message)
    {
        std::cerr << "RenegadeRuntimeScreenTests: " << message << '\n';
        return 1;
    }
}

int main()
{
    using namespace renegade;

    // User-owned accessibility preferences must round-trip independently of
    // every project's Screen/Flow assets and reject malformed saved data.
    const auto dir=std::filesystem::temp_directory_path()/
        std::filesystem::u8path("renegade-ui-accessibility-"+bridge::GenerateStableId());
    const auto preferenceFile=dir/"UiPrefs.v1.ini";
    runtime::RuntimeUiAccessibilityPreferences loaded;
    std::string preferenceError;
    if(!runtime::RuntimeUiAccessibilityPreferences::Load(preferenceFile,loaded,preferenceError)||
        loaded.reducedMotion)return Fail("missing accessibility file did not use defaults");
    runtime::RuntimeUiAccessibilityPreferences preferences;
    preferences.reducedMotion=true;
    if(!preferences.Save(preferenceFile,preferenceError)||
        !runtime::RuntimeUiAccessibilityPreferences::Load(preferenceFile,loaded,preferenceError)||
        !loaded.reducedMotion)return Fail("reduced motion preference did not persist");
    preferences.reducedMotion=false;
    if(!preferences.Save(preferenceFile,preferenceError)||
        !runtime::RuntimeUiAccessibilityPreferences::Load(preferenceFile,loaded,preferenceError)||
        loaded.reducedMotion)return Fail("preference overwrite did not persist");
    {std::ofstream invalid(preferenceFile,std::ios::trunc);invalid<<"renegade_ui_preferences=9\\nreduced_motion=1\\n";}
    if(runtime::RuntimeUiAccessibilityPreferences::Load(preferenceFile,loaded,preferenceError))
        return Fail("unknown accessibility preference version was accepted");
    // Old reduced-motion preferences migrate in memory without rewriting on read.
    {std::ofstream legacy(preferenceFile,std::ios::binary|std::ios::trunc);
        legacy<<"renegade_ui_preferences=1\r\nreduced_motion=1\r\n";}
    if(!runtime::RuntimeUiAccessibilityPreferences::Load(preferenceFile,loaded,preferenceError)||
        !loaded.reducedMotion||loaded.presentation.textScale!=1||loaded.presentation.contrast!=0)
        return Fail("legacy accessibility preference migration failed");
    preferences.presentation={1.75f,3,2,6,1.5f,44};
    if(!preferences.Save(preferenceFile,preferenceError)||
        !runtime::RuntimeUiAccessibilityPreferences::Load(preferenceFile,loaded,preferenceError)||
        loaded.presentation.textScale!=1.75f||loaded.presentation.contrast!=3||
        loaded.presentation.focusStyle!=2||loaded.presentation.focusWidth!=6||
        loaded.presentation.uiScale!=1.5f||loaded.presentation.minimumTargetSize!=44)
        return Fail("extended accessibility preferences did not persist");
    const auto unchanged=loaded;
    {std::ofstream bad(preferenceFile,std::ios::binary|std::ios::trunc);
        bad<<"renegade_ui_preferences=2\nreduced_motion=0\ntext_scale=9\ncontrast=0\nfocus_style=0\nfocus_width=2\n";}
    if(runtime::RuntimeUiAccessibilityPreferences::Load(preferenceFile,loaded,preferenceError)||
        loaded.presentation.textScale!=unchanged.presentation.textScale)
        return Fail("invalid preference load partially changed live settings");
    preferences.presentation.textScale=0;
    if(preferences.Save(preferenceFile,preferenceError))
        return Fail("invalid preference save accepted");
    for(const auto* action:{bridge::RuntimeUiTextScaleIncreaseAction,bridge::RuntimeUiTextScaleDecreaseAction,
        bridge::RuntimeUiTextScaleResetAction,bridge::RuntimeUiContrastCycleAction,
        bridge::RuntimeUiFocusColorCycleAction,bridge::RuntimeUiFocusWidthCycleAction,
        bridge::RuntimeUiScaleIncreaseAction,bridge::RuntimeUiScaleDecreaseAction,
        bridge::RuntimeUiScaleResetAction,bridge::RuntimeUiTargetSizeCycleAction})
        if(!bridge::IsRuntimeUiAccessibilityAction(action))
            return Fail("accessibility action incorrectly requires a Flow destination");
    std::error_code ignored;
    std::filesystem::remove_all(dir,ignored);

    bridge::ScreenDocument document = MakeScreen();
    std::string error;
    // G5H: transient native context buttons (never persisted to the authored
    // screen) can be safely hidden, shown, focused and dispatched.
    {
        auto context=MakeScreen();
        context.usage=bridge::ScreenUsage::Overlay;
        const char* actionIds[]={"inventory.context.use","inventory.context.drop_one",
            "inventory.context.drop_stack","inventory.context.take_one",
            "inventory.context.take_stack"};
        std::vector<std::string> ids;
        for(std::size_t i=0;i<5;++i) {
            auto button=context.widgets[2];
            button.id=bridge::GenerateStableId();
            button.name="Inventory Context";
            button.actionId=actionIds[i];
            button.text="ITEM ACTION";
            button.rect={973.f,172.f+42.f*i,232.f,38.f};
            button.visible=false;button.enabled=false;
            button.style=bridge::MakeScreenWidgetStyleTemplate(button.kind,38.f);
            context.widgets.push_back(button);
            context.actions.push_back({button.actionId});
            context.focusOrder.push_back(button.id);
            ids.push_back(button.id);
        }
        runtime::RuntimeScreenController controller;
        if(!controller.Initialize(context,error))
            return Fail(("G5H transient context Screen: "+error).c_str());
        if(controller.FocusWidget(ids[1]))
            return Fail("hidden context action accepted focus");
        if(!controller.SetWidgetState(ids[1],true,true,error) ||
           !controller.FocusWidget(ids[1]))
            return Fail("G5H active context button not focusable");
        runtime::RuntimeActionRequest request;
        if(!controller.MakeFocusedActionRequest(runtime::RuntimeInputSource::Keyboard,
            1,request,error) ||
           request.actionId!=actionIds[1])
            return Fail("G5H focused drop action dispatch mismatch");
        if(!controller.SetWidgetState(ids[1],false,false,error) ||
           controller.FocusWidget(ids[1]))
            return Fail("G5H closed context menu left hidden action enabled");
    }

    {
        bridge::ScreenAnimationTrack enter;enter.keys={{0,0},{.4,1}};
        bridge::ScreenAnimationTrack exit;exit.keys={{0,1},{.3,0}};
        bridge::ScreenTransitionPair policy{{enter},{exit}};
        bridge::ScreenTransitionPlayer transition;
        if(!transition.Start(policy,false,error)||transition.AcceptsInput()||transition.Frame().opacity!=0)
            return Fail("transition enter did not suppress input");
        transition.Tick(.2,false);const auto partial=transition.Frame().opacity;
        if(partial<=0||partial>=1||!transition.BeginExit(nullptr,false,error)||
            transition.Frame().opacity!=partial||transition.BeginExit(nullptr,false,error))
            return Fail("transition interruption snapped or accepted duplicate exit");
        transition.Tick(.3,false);
        if(!transition.ExitComplete()||transition.AcceptsInput()||transition.Frame().opacity!=0)
            return Fail("exit did not complete deterministically");
        transition.CancelExit();
        if(!transition.AcceptsInput()||transition.Frame().opacity!=1)
            return Fail("cancel did not restore active authored presentation");
        if(!transition.Start(policy,true,error)||!transition.AcceptsInput()||
            !transition.BeginExit(nullptr,true,error)||!transition.ExitComplete())
            return Fail("reduced motion did not complete transition immediately");
        transition.Reset();policy.enter[0].loop=bridge::ScreenAnimationLoop::Repeat;
        if(transition.Start(policy,false,error)||!transition.AcceptsInput())
            return Fail("invalid transition changed live lifecycle");
        runtime::RuntimeUiComposition composition;
        if(!composition.ReplacePrimary(document,true,error))return Fail(error.c_str());
        const auto identity=composition.PrimaryIdentity();
        if(!composition.PrimaryAcceptsInput()||!composition.BeginPrimaryExit(error)||
            composition.PrimaryAcceptsInput()||!composition.Resolve().gameplayPauseRequested||
            !composition.CancelPrimaryExit(error)||!composition.PrimaryAcceptsInput()||
            composition.PrimaryIdentity()!=identity)
            return Fail("exit/cancel lost modal pause/input/identity ownership");
        composition.ClearPrimary(); // the earlier modal intentionally shields lower HUD input
        auto hud=document;hud.usage=bridge::ScreenUsage::Hud;
        hud.composition=bridge::DefaultScreenCompositionPolicy(bridge::ScreenCompositionLayer::Modal);
        if(!composition.ReplaceHud(hud,true,error)||!composition.HudAcceptsInput()||
            !composition.BeginHudExit(error)||composition.HudAcceptsInput()||
            !composition.Resolve().gameplayPauseRequested||!composition.CancelHudExit(error)||
            !composition.HudAcceptsInput())
            return Fail("HUD exit/cancel lost modal input or pause ownership");
        bridge::ScreenTransitionPlayer cut;
        if(!cut.Start({},false,error)||!cut.BeginExit(nullptr,false,error)||!cut.ExitComplete())
            return Fail("empty transition did not complete immediately");
        cut.CancelExit();
        if(!cut.AcceptsInput())return Fail("immediate exit cancellation left controls disabled");
    }
    // UI-017: focus must match effective hierarchy visibility. Hiding a Group
    // cannot leave a descendant's action keyboard-activatable, and focus
    // recovery should advance to the next available control, not jump first.
    {
        auto focusScreen=MakeScreen();
        auto group=bridge::MakeScreenWidgetPreset(
            bridge::ScreenWidgetPreset::VerticalStack,1280,720);
        group.id=bridge::GenerateStableId();
        group.name="Focusable Group";
        group.rect={420,410,500,160};
        const auto playId=focusScreen.widgets[2].id;
        const auto quitId=focusScreen.widgets[3].id;
        focusScreen.widgets.insert(focusScreen.widgets.begin()+3,group);
        focusScreen.widgets[4].parentId=group.id;
        focusScreen.widgets[4].rect={20,20,400,76};
        auto about=focusScreen.widgets[2];
        about.id=bridge::GenerateStableId();
        about.name="About";about.text="ABOUT";
        about.actionId="about";about.rect={440,570,400,76};
        focusScreen.widgets.push_back(about);
        focusScreen.actions.push_back({"about"});
        focusScreen.focusOrder={playId,quitId,about.id};
        runtime::RuntimeScreenController focus;
        if(!focus.Initialize(focusScreen,error))
            return Fail("focus recovery fixture rejected");
        if(!focus.FocusedWidget()||focus.FocusedWidget()->id!=playId||
            !focus.FocusNext()||focus.FocusedWidget()->id!=quitId)
            return Fail("UI-017 focus fixture traversal incorrect");
        if(!focus.SetWidgetState(group.id,false,true,error)||
            !focus.FocusedWidget()||focus.FocusedWidget()->id!=about.id||
            focus.FocusWidget(quitId))
            return Fail("UI-017 hidden Group descendant retained focus");
        runtime::RuntimeActionRequest request;
        if(focus.MakeActionRequest(quitId,runtime::RuntimeInputSource::Keyboard,1,request,error))
            return Fail("UI-017 hidden Group child remained keyboard-activatable");
        if(!focus.SetWidgetState(group.id,true,true,error)||
            !focus.FocusPrevious()||focus.FocusedWidget()->id!=quitId||
            !focus.SetWidgetState(quitId,false,true,error)||
            !focus.FocusedWidget()||focus.FocusedWidget()->id!=about.id)
            return Fail("UI-017 next-in-order focus recovery after dynamic hide failed");
    }

    runtime::RuntimeScreenController controller;
    if (!controller.Initialize(document, error))
    {
        return Fail("valid controller document was rejected");
    }

    const std::string playId = document.widgets[2].id;
    const std::string quitId = document.widgets[3].id;
    if (controller.FocusedWidget() == nullptr ||
        controller.FocusedWidget()->id != playId)
    {
        return Fail("initial focus was not deterministic");
    }
    if (!controller.FocusNext() ||
        controller.FocusedWidget() == nullptr ||
        controller.FocusedWidget()->id != quitId ||
        !controller.FocusPrevious() ||
        controller.FocusedWidget()->id != playId)
    {
        return Fail("next/previous focus order was not deterministic");
    }

    runtime::RuntimeActionDispatcher dispatcher;
    int playCount = 0;
    int quitCount = 0;
    if (!dispatcher.Register(
            bridge::RuntimeScreenPlayAction,
            [&playCount](const runtime::RuntimeActionRequest& request)
            {
                ++playCount;
                return runtime::RuntimeActionResult{
                    true,
                    runtime::RuntimeActionCode::Success,
                    request,
                    "play entered LP02 boundary",
                };
            },
            error) ||
        !dispatcher.Register(
            bridge::RuntimeScreenQuitAction,
            [&quitCount](const runtime::RuntimeActionRequest& request)
            {
                ++quitCount;
                return runtime::RuntimeActionResult{
                    true,
                    runtime::RuntimeActionCode::QuitRequested,
                    request,
                    "quit requested normal shutdown",
                };
            },
            error))
    {
        return Fail("could not register bounded actions");
    }

    runtime::RuntimeActionRequest request;
    if (!controller.MakeFocusedActionRequest(
            runtime::RuntimeInputSource::Keyboard,
            1,
            request,
            error))
    {
        return Fail("keyboard activation did not produce an action request");
    }
    auto result = dispatcher.Dispatch(request);
    if (!result.succeeded || result.request.actionId != "play" ||
        result.request.inputSource != runtime::RuntimeInputSource::Keyboard ||
        playCount != 1)
    {
        return Fail("keyboard did not enter the stable play dispatcher");
    }

    if (!controller.FocusNext() ||
        !controller.MakeFocusedActionRequest(
            runtime::RuntimeInputSource::Gamepad,
            2,
            request,
            error))
    {
        return Fail("gamepad activation did not produce an action request");
    }
    result = dispatcher.Dispatch(request);
    if (!result.succeeded || result.request.actionId != "quit" ||
        result.request.inputSource != runtime::RuntimeInputSource::Gamepad ||
        quitCount != 1)
    {
        return Fail("gamepad did not enter the stable quit dispatcher");
    }

    if (!controller.MakeActionRequest(
            playId,
            runtime::RuntimeInputSource::Mouse,
            3,
            request,
            error))
    {
        return Fail("mouse activation did not produce an action request");
    }
    result = dispatcher.Dispatch(request);
    if (!result.succeeded || result.request.actionId != "play" ||
        result.request.inputSource != runtime::RuntimeInputSource::Mouse ||
        playCount != 2)
    {
        return Fail("mouse did not enter the same stable dispatcher");
    }

    if (!controller.SetWidgetState(playId, false, true, error) ||
        controller.FocusedWidget() == nullptr ||
        controller.FocusedWidget()->id != quitId)
    {
        return Fail("hidden control was not skipped and focus repaired");
    }
    if (!controller.SetWidgetState(quitId, true, false, error) ||
        controller.HasFocus())
    {
        return Fail("disabled controls were not skipped");
    }
    if (!controller.SetWidgetState(playId, true, true, error) ||
        controller.FocusedWidget() == nullptr ||
        controller.FocusedWidget()->id != playId)
    {
        return Fail("focus did not recover when a control became available");
    }
    if (!controller.SetWidgetState(playId, false, true, error) ||
        controller.MakeActionRequest(
            playId,
            runtime::RuntimeInputSource::Mouse,
            4,
            request,
            error))
    {
        return Fail("hidden control activation did not fail closed");
    }

    runtime::RuntimeActionRequest unknown;
    unknown.actionId = "unknown.action";
    unknown.widgetId = playId;
    unknown.inputSource = runtime::RuntimeInputSource::Test;
    unknown.sequence = 5;
    result = dispatcher.Dispatch(unknown);
    if (result.succeeded ||
        result.code != runtime::RuntimeActionCode::UnknownAction ||
        result.message.find("Unknown Runtime action") == std::string::npos)
    {
        return Fail("unknown action did not produce a structured failure");
    }

    if (dispatcher.Register(
            bridge::RuntimeScreenPlayAction,
            [](const runtime::RuntimeActionRequest& request)
            {
                return runtime::RuntimeActionResult{
                    true,
                    runtime::RuntimeActionCode::Success,
                    request,
                    {},
                };
            },
            error))
    {
        return Fail("duplicate action registration was accepted");
    }


    auto controlDoc=MakeScreen();std::vector<bridge::StableId> ids;
    for(auto preset:{bridge::ScreenWidgetPreset::Toggle,bridge::ScreenWidgetPreset::Slider,
        bridge::ScreenWidgetPreset::Selector,bridge::ScreenWidgetPreset::TextInput})
    {
        auto w=bridge::MakeScreenWidgetPreset(preset,1280,720);w.id=bridge::GenerateStableId();
        if(w.kind==bridge::ScreenWidgetKind::TextInput)w.control.textLimit=3;
        ids.push_back(w.id);controlDoc.focusOrder.push_back(w.id);controlDoc.widgets.push_back(w);
    }
    runtime::RuntimeScreenController controls;
    if(!controls.Initialize(controlDoc,error)||!controls.FocusWidget(ids[0])||
       !controls.SetControlValue(ids[0],true,error)||!std::get<bool>(controls.FindWidget(ids[0])->control.value)||
       std::get<bool>(controls.AuthoredDocument().widgets[4].control.value))
        return Fail("local UI value changed authored fallback or failed focus");
    if(controls.MakeActionRequest(ids[0],runtime::RuntimeInputSource::Test,1,request,error))
        return Fail("local Toggle emitted an implicit Flow action");
    if(!controls.SetControlValue(ids[1],.8,error)||controls.SetControlValue(ids[1],2.0,error)||
       controls.SetControlValue(ids[1],true,error)||controls.SetControlValue(ids[2],99.0,error)||
       !controls.SetControlValue(ids[2],1.0,error)||!controls.SetControlValue(ids[3],std::string("ABC"),error)||
       controls.SetControlValue(ids[3],std::string("ABCD"),error))
        return Fail("local UI type/range/text limits failed");
    bridge::ScreenBindingSnapshot controlSnapshot;controlSnapshot.session=1;controlSnapshot.revision=1;
    bridge::ScreenBindingContext controlContext;controlContext.session=1;
    if(!controls.ApplyBindings(controlSnapshot,controlContext,error)||
       std::get<double>(controls.FindWidget(ids[1])->control.value)!=.8||
       !controls.SetWidgetState(ids[0],false,true,error)||controls.SetControlValue(ids[0],false,error))
        return Fail("binding frame discarded local UI state or hidden control accepted input");
    if(!controls.Initialize(controlDoc,error)||std::get<bool>(controls.FindWidget(ids[0])->control.value)||
       std::get<double>(controls.FindWidget(ids[1])->control.value)!=.5)
        return Fail("UI instance restart did not reset authored initial values");

    std::cout << "PASS: LP03 Runtime focus and stable action dispatch\n";

    {
        using namespace bridge;
        auto local=MakeScreen();
        auto toggle=MakeScreenWidgetPreset(ScreenWidgetPreset::Toggle,1280,720);toggle.id=GenerateStableId();
        auto slider=MakeScreenWidgetPreset(ScreenWidgetPreset::Slider,1280,720);slider.id=GenerateStableId();
        local.widgets.push_back(toggle);local.widgets.push_back(slider);
        local.focusOrder.push_back(toggle.id);local.focusOrder.push_back(slider.id);
        const auto title=local.widgets[1].id;
        local.widgets[4].interactions={
            {ScreenInteractionEvent::ValueChanged,ScreenLocalActionKind::SetValue,slider.id,.8,ScreenLocalCondition{toggle.id,ScreenLocalComparison::Equal,true}},
            {ScreenInteractionEvent::ValueChanged,ScreenLocalActionKind::SetVisible,title,false,ScreenLocalCondition{slider.id,ScreenLocalComparison::GreaterEqual,.8}},
            {ScreenInteractionEvent::ValueChanged,ScreenLocalActionKind::Focus,slider.id,true,{}}
        };
        runtime::RuntimeScreenController a,b;
        if(!a.Initialize(local,error)||!b.Initialize(local,error)||!a.SetControlValue(toggle.id,true,error)||
            std::get<double>(a.FindWidget(slider.id)->control.value)!=.8||a.FindWidget(title)->visible||
            a.FocusedWidget()->id!=slider.id||!b.FindWidget(title)->visible||
            std::get<double>(b.FindWidget(slider.id)->control.value)!=.5||
            std::get<bool>(a.AuthoredDocument().widgets[4].control.value))
            return Fail("ordered value-change transaction/isolation/focus failed");
        // A later invalid final focus rolls back the initiating value and all preceding steps.
        auto rejected=local;
        rejected.widgets[4].interactions.push_back({ScreenInteractionEvent::ValueChanged,ScreenLocalActionKind::SetVisible,slider.id,false,{}});
        if(!a.Initialize(rejected,error))return Fail("valid rollback fixture rejected");
        if(a.SetControlValue(toggle.id,true,error)||std::get<bool>(a.FindWidget(toggle.id)->control.value)||
            std::get<double>(a.FindWidget(slider.id)->control.value)!=.5||!a.FindWidget(title)->visible)
            return Fail("failed local action list partially mutated UI state");
        if(!a.Initialize(local,error)||std::get<bool>(a.FindWidget(toggle.id)->control.value)||
            std::get<double>(a.FindWidget(slider.id)->control.value)!=.5)
            return Fail("local actions survived instance reset");
        auto bound=local;ScreenBinding binding;binding.property=ScreenBindingProperty::Visible;
        binding.source.attributeId="health.current";binding.operation=ScreenBindingOperation::AtLeast;
        bound.widgets[1].bindings={binding};
        if(ValidateScreenDocument(bound,bound.envelope.projectId,error))
            return Fail("local action overwrote gameplay-bound visibility");
    }
    {
        // G4-B2: exercise the actual RuntimeScreenController activation path,
        // not just a standalone InventoryScreenAdapter direct transfer.
        bridge::InventoryService inventory;
        if (inventory.RegisterItem({"potion","Potion",10,0.2,{}})!=bridge::InventoryResult::Success ||
            inventory.CreateContainer("player",12,5)!=bridge::InventoryResult::Success ||
            inventory.CreateContainer("chest",12,5)!=bridge::InventoryResult::Success ||
            inventory.Add("player","potion",3)!=bridge::InventoryResult::Success)
            return Fail("G4B2 inventory Screen input fixture rejected");
        auto screen=bridge::MakeInventoryStarterScreen(
            bridge::GenerateStableId(),"Content/UI/InventoryInputProof.renegade-screen",2);
        screen.enabledInGame=true;
        const auto view=bridge::BuildInventoryOverlayView(inventory,
            {{"player","Player"},{"chest","Chest"}},2);
        bridge::ScreenDocument projected;
        if(!bridge::ProjectInventoryIntoScreen(screen,view,projected,error))
            return Fail("G4B2 runtime projected Screen invalid");
        runtime::RuntimeScreenController inventoryScreen;
        if(!inventoryScreen.Initialize(projected,error))
            return Fail("G4B2 controller rejected projected Screen");
        const auto& slot=projected.widgets[3];
        if(slot.localOnly||!slot.enabled||slot.actionId!="inventory.slot.0.0")
            return Fail("G4B2 interactive slot is locally suppressed");
        runtime::RuntimeActionRequest slotRequest;
        if(!inventoryScreen.MakeActionRequest(slot.id,runtime::RuntimeInputSource::Mouse,
               1,slotRequest,error) ||
           slotRequest.actionId!=slot.actionId ||
           !inventoryScreen.ExecuteInteractions(slot.id,
               bridge::ScreenInteractionEvent::Activate,error))
            return Fail("G4B2 Screen controller click command was blocked");
        bridge::InventoryGameplayGateway grants(inventory);
        grants.Grant("player",{true,true,true});
        grants.Grant("chest",{true,true,true});
        bridge::InventorySelectionController selection;
        if(bridge::ExecuteInventoryScreenSlotAction(slotRequest.actionId,view,
               selection,grants)!=bridge::InventoryResult::Success ||
           inventory.FindContainer("player")->stacks[0].quantity!=2 ||
           inventory.FindContainer("chest")->stacks[0].quantity!=1)
            return Fail("G4B2 Screen controller click transfer did not mutate gameplay");

        // G4-B2C: reproduce the owner's stale UI bug using the exact Runtime
        // presentation-update sequence: SetWidgetState rebuilds the *whole*
        // binding frame for every slot, then SetWidgetDisplayText updates
        // one slot. Previously, the next slot reset all earlier labels.
        const auto applyLiveInventory=[&]() -> bool {
            const auto live=bridge::BuildInventoryOverlayView(inventory,
                {{"player","Player"},{"chest","Chest"}},2);
            bridge::ScreenDocument next;
            if(!bridge::ProjectInventoryIntoScreen(screen,live,next,error))
                return false;
            for(const auto& item:next.widgets) {
                if(item.kind!=bridge::ScreenWidgetKind::Button ||
                   item.actionId.rfind("inventory.slot.",0)!=0)
                    continue;
                if(!inventoryScreen.SetWidgetState(
                      item.id,item.visible,item.enabled,error) ||
                   !inventoryScreen.SetWidgetDisplayText(
                      item.id,item.text,error))
                    return false;
            }
            return true;
        };
        const auto playerSlot=projected.widgets[3].id;
        const auto chestSlot=projected.widgets[6].id; // two slots per panel
        const auto statusSlot=projected.widgets.back().id;
        if(!inventoryScreen.SetWidgetDisplayText(
              statusSlot,"MOVED ONE ITEM",error))
            return Fail("G4B2C could not set transient transfer status");
        if(!applyLiveInventory() ||
           inventoryScreen.FindWidget(playerSlot)->text!="Potion  x2" ||
           inventoryScreen.FindWidget(chestSlot)->text!="Potion  x1")
            return Fail("G4B2C transient labels reverted after adjacent widget state update");

        const auto playerId=inventory.FindContainer("player")->stacks[0].instanceId;
        if(inventory.Transfer("player","chest",playerId,2)!=bridge::InventoryResult::Success ||
           !applyLiveInventory() ||
           inventoryScreen.FindWidget(playerSlot)->enabled ||
           inventoryScreen.FindWidget(playerSlot)->text!="Empty slot" ||
           inventoryScreen.FindWidget(chestSlot)->text!="Potion  x3")
            return Fail("G4B2C E/Lua depleted Player but Screen still showed original items");

        const auto chestId=inventory.FindContainer("chest")->stacks[0].instanceId;
        if(inventory.Transfer("chest","player",chestId,1)!=bridge::InventoryResult::Success ||
           !applyLiveInventory() ||
           !inventoryScreen.FindWidget(playerSlot)->enabled ||
           inventoryScreen.FindWidget(playerSlot)->text!="Potion  x1" ||
           inventoryScreen.FindWidget(chestSlot)->text!="Potion  x2")
            return Fail("G4B2C chest-to-Player Screen refresh lost updated counts");
        // A later binding/state refresh must not revert the live display.
        if(!inventoryScreen.SetWidgetState(playerSlot,true,true,error) ||
           inventoryScreen.FindWidget(playerSlot)->text!="Potion  x1" ||
           inventoryScreen.FindWidget(chestSlot)->text!="Potion  x2" ||
           inventoryScreen.FindWidget(statusSlot)->text!="MOVED ONE ITEM")
            return Fail("G4B2C binding frame silently clobbered live text or status");
        if(!inventoryScreen.Initialize(projected,error) ||
           inventoryScreen.FindWidget(playerSlot)->text!="Potion  x3" ||
           inventoryScreen.FindWidget(chestSlot)->text!="Empty slot" ||
           inventoryScreen.FindWidget(statusSlot)->text=="MOVED ONE ITEM")
            return Fail("G4B2C presentation overrides leaked across Screen reset");
    }
    {
        bridge::ScreenWidget slot;slot.kind=bridge::ScreenWidgetKind::Button;
        for(const bool legacy:{false,true}) {
            slot.localOnly=legacy;
            for(const char* action:{"inventory.paged.0.slot.0","inventory.paged.1.scroll.down"}) {
                slot.actionId=action;
                if(!runtime::ShouldDispatchScreenAction(slot,true) ||
                   runtime::ShouldDispatchScreenAction(slot,false))
                    return Fail("inventory local click/page dispatch lost or leaked into navigation");
            }
        }
        slot.actionId="new_game";slot.localOnly=false;
        if(!runtime::ShouldDispatchScreenAction(slot,false))return Fail("genuine navigation was suppressed");
        slot.localOnly=true;
        if(runtime::ShouldDispatchScreenAction(slot,true))return Fail("unrelated local action escaped its host");
    }
    return 0;
}
