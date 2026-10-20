#include "renegade/bridge/ScreenAuthoringSession.h"
#include "../Runtime/src/RuntimeUiComposition.h"
#include <filesystem>
#include <fstream>
#include <iostream>
using namespace renegade::bridge;
int main()
{
    int failures=0;std::string error;
    const auto check=[&](bool ok,const char* message){if(!ok){++failures;std::cerr<<message<<": "<<error<<'\n';}};
    const auto root=std::filesystem::temp_directory_path()/("renegade-composition-"+GenerateStableId());
    struct Cleanup{std::filesystem::path p;~Cleanup(){std::error_code e;std::filesystem::remove_all(p,e);}} cleanup{root};
    std::filesystem::create_directories(root);
    const auto project=GenerateStableId();const auto path=(root/"Hud.renegade-screen").generic_u8string();
    ScreenDocument document;document.usage=ScreenUsage::Hud;
    document.envelope=CreateDocumentEnvelope(project,RuntimeScreenDocumentType,"Content/UI/Hud.renegade-screen","Composition proof");
    check(WriteScreenDocument(path,document,error),"Fixture write failed");
    ScreenAuthoringSession session;check(session.Open(path,project,error),"Session open failed");
    auto policy=DefaultScreenCompositionPolicy(ScreenCompositionLayer::Overlay);
    policy.cursor=ScreenCursorPolicy::Show;policy.order=17;policy.showHudUnderlay=false;
    const auto before=session.UndoCount();
    check(session.UpdateScreenComposition(policy,error) && session.UndoCount()==before+1 && session.Save(error),"Policy was not one saved command");
    ScreenDocument reopened;check(ReadScreenDocument(path,project,reopened,error),"Policy reopen failed");
    check(reopened.composition && reopened.composition->order==17 && reopened.composition->cursor==ScreenCursorPolicy::Show &&
        reopened.composition->uiInput==ScreenUiInputPolicy::Controls && !reopened.composition->pauseGameplay && !reopened.composition->showHudUnderlay,"Saved override lost fields");
    renegade::runtime::RuntimeUiComposition runtime;
    check(runtime.ReplacePrimary(reopened,true,error) && runtime.PrimaryAcceptsInput() &&
        !runtime.Resolve().gameplayPauseRequested && runtime.Resolve().cursor==ScreenCursorPolicy::Show,"Runtime ignored authored policy");
    check(session.Undo(error) && !session.Document().composition && session.Redo(error) &&
        session.Document().composition && session.Document().composition->order==17,"Policy Undo/Redo failed");
    const auto revision=session.Revision();policy.uiInput=ScreenUiInputPolicy::Modal;policy.blockGameplayInput=false;
    check(!session.UpdateScreenComposition(policy,error) && session.Revision()==revision,"Invalid modal policy mutated history");
    check(session.UpdateScreenComposition(std::nullopt,error) && session.Save(error) &&
        ReadScreenDocument(path,project,reopened,error) && !reopened.composition,"Usage default reset did not persist");
    check(runtime.ReplacePrimary(reopened,false,error) && !runtime.PrimaryAcceptsInput() &&
        !runtime.Resolve().gameplayPauseRequested,"Default HUD captured input after reset");
    // Strict disk parsing must not coerce corrupt fields into working defaults.
    policy=DefaultScreenCompositionPolicy(ScreenCompositionLayer::Modal);document.composition=policy;
    check(WriteScreenDocument(path,document,error),"Malformed fixture setup failed");
    std::ifstream input(path,std::ios::binary);std::string bytes{std::istreambuf_iterator<char>(input),{}};input.close();
    const auto field=bytes.find("ui_input = 2");check(field!=std::string::npos,"Expected policy field missing");
    if(field!=std::string::npos)bytes.replace(field,12,"ui_input = x");
    std::ofstream output(path,std::ios::binary|std::ios::trunc);output<<bytes;output.close();
    const auto sentinel=reopened.envelope.documentId;
    check(!ReadScreenDocument(path,project,reopened,error) && reopened.envelope.documentId==sentinel,"Corrupt policy was accepted or partially assigned");
    if(failures)return 1;
    std::cout<<"PASS: composition persistence, authored Runtime policy, Undo/Redo, reset and strict corrupt-input rejection\n";
}
