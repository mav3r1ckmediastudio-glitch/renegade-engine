#include "../Runtime/src/RuntimeScreen.h"
#include <iostream>
using namespace renegade::bridge;
using namespace renegade::runtime;
int main()
{
    int failures=0;
    const auto check=[&](bool valid,const char* message) {
        if (!valid) { ++failures; std::cerr<<message<<'\n'; }
    };
    ScreenDocument document;
    document.envelope=CreateDocumentEnvelope(GenerateStableId(),RuntimeScreenDocumentType,
        "Content/UI/Menu.renegade-screen","Runtime binding integration");
    ScreenWidget button;
    button.id=GenerateStableId();button.name="Continue";button.kind=ScreenWidgetKind::Button;
    button.rect={100,100,300,60};button.text="Continue";button.actionId="continue";
    button.style=MakeScreenWidgetStyleTemplate(button.kind,60);
    ScreenBinding enabled;
    enabled.property=ScreenBindingProperty::Enabled;enabled.source.attributeId="alive";
    button.bindings={enabled};
    ScreenWidget text;
    text.id=GenerateStableId();text.name="Label";text.text="Fallback";
    text.rect={100,200,300,60};text.style=MakeScreenWidgetStyleTemplate(text.kind,60);
    ScreenBinding label;label.source.attributeId="label";
    text.bindings={label};
    ScreenWidget progress;
    progress.id=GenerateStableId();progress.name="Health";progress.kind=ScreenWidgetKind::Progress;
    progress.rect={100,300,300,30};progress.style=MakeScreenWidgetStyleTemplate(progress.kind,30);
    ScreenBinding fill;fill.property=ScreenBindingProperty::Fill;
    fill.operation=ScreenBindingOperation::Ratio;fill.source.attributeId="health.current";
    fill.denominator.attributeId="health.maximum";progress.bindings={fill};
    document.widgets={button,text,progress};document.actions={{"continue"}};
    document.focusOrder={button.id};
    RuntimeScreenController controller;
    std::string error;
    check(controller.Initialize(document,error),"Controller initialization failed");
    ScreenBindingSnapshot snapshot{50,1,{{enabled.source,false,3},
        {label.source,std::string("HP 10"),3},{fill.source,10.0,3},{fill.denominator,100.0,3}}};
    ScreenBindingContext context;context.session=50;
    context.generations[{ScreenBindingScope::PossessedPlayer,{}}]=3;
    check(controller.ApplyBindings(snapshot,context,error) && !controller.HasFocus() &&
        controller.BindingFrame().ready,"Disabled binding did not suppress focus");
    RuntimeActionRequest request;
    check(!controller.MakeActionRequest(button.id,RuntimeInputSource::Keyboard,1,request,error),
        "A dynamically disabled button activated");
    check(controller.FindWidget(text.id)->text=="HP 10" &&
        controller.FindWidget(progress.id)->fill==.1f,"Resolved text/fill did not reach Runtime document");
    check(controller.SetWidgetState(progress.id,false,true,error),"Unbound local state override failed");
    snapshot.revision=2;snapshot.values[0].value=true;snapshot.values[1].value=std::string{};
    snapshot.values[2].value=50.0;
    check(controller.ApplyBindings(snapshot,context,error) && controller.HasFocus() &&
        controller.FindWidget(text.id)->text.empty() &&
        controller.FindWidget(progress.id)->fill==.5f &&
        !controller.FindWidget(progress.id)->visible,
        "Fresh bindings lost local state or rejected valid empty transient text");
    check(controller.MakeActionRequest(button.id,RuntimeInputSource::Keyboard,2,request,error) &&
        request.actionId=="continue","Binding evaluation changed symbolic Flow outcome");
    check(!controller.SetWidgetState(button.id,true,false,error),
        "A local write silently overrode a bound property");
    snapshot.revision=1;snapshot.values[2].value=99.0;
    check(!controller.ApplyBindings(snapshot,context,error) &&
        controller.FindWidget(progress.id)->fill==.5f,"Late snapshot replaced current Runtime presentation");
    context.session=snapshot.session=51;
    check(controller.ApplyBindings(snapshot,context,error),"New Runtime session was rejected");
    context.session=snapshot.session=50;snapshot.revision=100;
    check(!controller.ApplyBindings(snapshot,context,error),"Old session reactivated after a newer session");
    check(controller.AuthoredDocument().widgets[1].text=="Fallback" &&
        controller.AuthoredDocument().widgets[2].fill==.5f &&
        controller.AuthoredDocument().widgets[2].visible,
        "Runtime presentation mutated authored fallback values");
    if(failures)return 1;
    std::cout<<"PASS: Runtime binding presentation, input suppression, local state and lifecycle rejection\n";
}
