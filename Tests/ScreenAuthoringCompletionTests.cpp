#include "renegade/bridge/ScreenAuthoringSession.h"
#include "renegade/bridge/ScreenWidgetPresets.h"
#include "renegade/bridge/ScreenGridPreferences.h"
#include "renegade/bridge/ScreenFlowDestinationPlan.h"
#include "renegade/bridge/StoryFlowAuthoringSession.h"
#include "../Studio/src/StoryFlowScreenEditorHandoff.h"
#include "../Studio/src/ScreenEditorUndoTimeline.h"
#include "../Studio/src/ScreenEditorWorkspacePreferences.h"
#include "../Studio/src/ScreenEditorLibraryPalette.h"
#include <iostream>
#include <filesystem>
#include <fstream>
using namespace renegade::bridge;
using namespace renegade::studio;
int main(){
 int failures=0;std::string error;
 auto check=[&](bool ok,const char* text){if(!ok){++failures;std::cerr<<text<<": "<<error<<'\n';}};
 const auto root=std::filesystem::temp_directory_path()/("ac-"+GenerateStableId().substr(0,8));
 struct Cleanup{std::filesystem::path root;~Cleanup(){std::error_code e;std::filesystem::remove_all(root,e);}} cleanup{root};
 std::filesystem::create_directories(root/"Content/Screens");const auto project=GenerateStableId();
 ScreenDocument doc;doc.usage=ScreenUsage::Hud;doc.envelope=CreateDocumentEnvelope(project,RuntimeScreenDocumentType,"Content/Screens/A.renegade-screen","Authoring completion");
 for(auto preset:{ScreenWidgetPreset::Panel,ScreenWidgetPreset::Toggle,ScreenWidgetPreset::Slider,ScreenWidgetPreset::Selector,ScreenWidgetPreset::TextInput}){
  auto w=MakeScreenWidgetPreset(preset,1280,720);w.id=GenerateStableId();w.name="Control "+std::to_string(doc.widgets.size());w.rect={float(50+doc.widgets.size()*180),100,100,40};
  if(preset==ScreenWidgetPreset::Panel){w.kind=ScreenWidgetKind::Button;w.enabled=true;w.text="Activate";w.style=MakeScreenWidgetStyleTemplate(w.kind,w.rect.height);w.actionId="activate";doc.actions.push_back({w.actionId});}
  doc.focusOrder.push_back(w.id);doc.widgets.push_back(w);
 }
 const auto path=root/doc.envelope.pathHint;check(WriteScreenDocument(path.generic_u8string(),doc,error),"fixture save");
 ScreenAuthoringSession session;if(!session.Open(path.generic_u8string(),project,error)){std::cerr<<error;return 1;}
 // Every supported non-Button can cross the Button in focus order, undo, redo, save and reopen.
 for(std::size_t n=1;n<doc.widgets.size();++n){const auto before=session.Document().focusOrder;const auto id=doc.widgets[n].id;
  check(session.MoveButtonFocusEarlier(id,error),"mixed control focus up");const auto after=session.Document().focusOrder;
  check(before!=after&&session.Undo(error)&&session.Document().focusOrder==before&&session.Redo(error)&&session.Document().focusOrder==after,"mixed focus history");
  check(session.MoveButtonFocusLater(id,error),"mixed control focus down");
 }
 check(session.Save(error)&&session.Reload(error)&&session.Document().focusOrder==doc.focusOrder,"mixed focus reopen");
 // Library handoff shares document identity with Journey while retaining only unique contexts.
 FlowDocument flow;flow.envelope=CreateDocumentEnvelope(project,StoryFlowDocumentType,"Content/Flow/Main.renegade-flow","Routing contexts");
 FlowNode start;start.id=GenerateStableId();start.name="Game Start";start.kind=FlowNodeKind::GameStart;flow.startNodeId=start.id;
 FlowNode source;source.id=GenerateStableId();source.name="Distinct Journey Name";source.kind=FlowNodeKind::Screen;source.screenDocumentId=doc.envelope.documentId;source.screenPathHint=doc.envelope.pathHint;
 FlowNode quit;quit.id=GenerateStableId();quit.name="Quit";quit.kind=FlowNodeKind::Quit;
 FlowNode level;level.id=GenerateStableId();level.name="Level";level.kind=FlowNodeKind::Level;level.sceneAssetId=GenerateStableId();level.scenePathHint="Content/Scenes/Main.wiscene";
 flow.nodes={start,source,quit,level};FlowRoute boot;boot.id=GenerateStableId();boot.sourceNodeId=start.id;boot.outcome=GameStartOutcome;boot.destinationNodeId=source.id;flow.routes={boot};
 StoryFlowScreenEditorHandoff handoff;check(BuildStandaloneScreenEditorHandoff(root.generic_u8string(),project,doc.envelope.documentId,doc.envelope.pathHint,handoff,error),"library resolve identity");
 auto contexts=BindLibraryFlowContext(flow,handoff);check(contexts.size()==1&&handoff.screenNodeId==source.id,"unique Library Flow context lost");
 auto shared=source;shared.id=GenerateStableId();shared.name="Options in Pause";flow.nodes.push_back(shared);contexts=BindLibraryFlowContext(flow,handoff);check(contexts.size()==2&&handoff.screenNodeId.empty(),"shared Library picked first context");
 auto standalone=handoff;standalone.screenDocumentId=GenerateStableId();check(BindLibraryFlowContext(flow,standalone).empty()&&standalone.screenNodeId.empty(),"standalone guessed Flow context");
 const auto selectedPlan=PlanButtonOpenScreen(doc,flow,shared.id,doc.widgets[0].id,quit.id);
 check(selectedPlan.succeeded&&selectedPlan.candidateFlow.routes.back().sourceNodeId==shared.id,"explicit shared context route");
 const auto directPlan=PlanButtonOpenScreen(doc,flow,source.id,doc.widgets[0].id,quit.id);
 check(directPlan.succeeded&&directPlan.candidateFlow.routes.back().sourceNodeId==source.id,"Journey route parity");
 const auto levelPlan=PlanButtonOpenScreen(doc,flow,source.id,doc.widgets[0].id,level.id);
 check(levelPlan.succeeded&&levelPlan.candidateFlow.routes.back().destinationEntry=="player_entry","governed Level entry default");
 auto custom=levelPlan.candidateFlow;custom.routes.back().destinationEntry="custom_entry";
 check(PlanButtonOpenScreen(doc,custom,source.id,doc.widgets[0].id,level.id).noChanges,"existing Level entry overwritten");
 check(!PlanButtonOpenScreen(doc,flow,source.id,doc.widgets[0].id,start.id).succeeded,"Game Start eligible as destination");
 const auto fp=root/flow.envelope.pathHint;std::filesystem::create_directories(fp.parent_path());check(WriteFlowDocument(fp.generic_u8string(),flow,error),"Flow fixture save");StoryFlowAuthoringSession flowSession;check(flowSession.Open(fp.generic_u8string(),project,error),"Flow fixture open");
 // Actual bounded session history interleaved with route dispatch, including a divergent branch.
 ScreenEditorUndoTimeline timeline;using Kind=ScreenEditorUndoTimeline::Kind;
 timeline.Reset(session.UndoCount(),session.RedoCount(),session.HistoryMutation());
 auto observe=[&]{timeline.ObserveScreen(session.UndoCount(),session.RedoCount(),session.HistoryMutation());};
 for(int i=0;i<300;++i){auto w=*session.FindWidget(doc.widgets[0].id);ScreenWidgetAuthoringEdit e{w.name+"x",w.text,w.rect,w.visible,w.enabled};check(session.UpdateWidget(w.id,e,error),"saturating edit");observe();}
 check(session.UndoCount()==255,"history not saturated");FlowRoute actualRoute=directPlan.candidateFlow.routes.back();StableId createdRoute;
 check(flowSession.AddRoute(actualRoute,createdRoute,error),"actual Flow route transaction");timeline.RecordFlowRoute();
 auto w=*session.FindWidget(doc.widgets[0].id);check(session.UpdateWidget(w.id,{"After route",w.text,w.rect,w.visible,w.enabled},error),"post-route edit");observe();
 check(timeline.NextUndo()==Kind::Screen,"saturation dispatched older route first");check(session.Undo(error),"screen undo");timeline.DidUndo(session.UndoCount(),session.RedoCount(),session.HistoryMutation());
 check(timeline.NextUndo()==Kind::FlowRoute&&flowSession.Undo(error)&&flowSession.Document().routes.size()==1,"route chronology/actual Undo lost");timeline.DidUndo(session.UndoCount(),session.RedoCount(),session.HistoryMutation());
 check(timeline.NextRedo()==Kind::FlowRoute&&flowSession.Redo(error)&&flowSession.Document().routes.size()==2,"actual Flow redo ordering");timeline.DidRedo(session.UndoCount(),session.RedoCount(),session.HistoryMutation());
 check(session.Redo(error),"screen redo");timeline.DidRedo(session.UndoCount(),session.RedoCount(),session.HistoryMutation());
 check(session.Undo(error),"branch undo");timeline.DidUndo(session.UndoCount(),session.RedoCount(),session.HistoryMutation());
 w=*session.FindWidget(doc.widgets[0].id);check(session.UpdateWidget(w.id,{"Divergent",w.text,w.rect,w.visible,w.enabled},error),"branch edit");observe();check(timeline.NextRedo()==Kind::None&&session.RedoCount()==0,"divergent redo retained");
 check(flowSession.Save(error)&&flowSession.Reload(error)&&flowSession.Document().routes.back().destinationNodeId==quit.id,"context route save reload");
 // Proportional batch transform is captured once, coalesced, atomic and cancellable.
 const std::vector<StableId> ids{doc.widgets[0].id,doc.widgets[1].id};ScreenAuthoringSession::GeometryCapture capture;ScreenRect bounds;
 check(session.CaptureGeometry(ids,capture,bounds,error),"capture siblings");const auto history=session.UndoCount();
 session.BeginCoalescedEdit();auto target=bounds;target.width*=1.5f;target.height*=2;
 check(session.TransformGeometry(capture,bounds,target,error),"batch resize");target.width*=1.1f;check(session.TransformGeometry(capture,bounds,target,error),"second gesture frame");session.EndCoalescedEdit();
 ScreenRect first,second;check(ResolveScreenWidgetRect(session.Document(),ids[0],first,error)&&ResolveScreenWidgetRect(session.Document(),ids[1],second,error),"resolve resized controls");
 check(std::abs(first.width-165)<.01f&&first.height==80&&std::abs(second.x-(bounds.x+180*1.65f))<.01f,"proportional size and spacing");
 check(session.UndoCount()==history&&session.Undo(error),"saturated coalesced undo"); // At capacity the count remains bounded.
 check(session.FindWidget(ids[0])->rect.width==100&&session.Redo(error)&&session.FindWidget(ids[0])->rect.width>164.99f&&session.FindWidget(ids[0])->rect.width<165.01f,"batch undo redo");
 check(session.CaptureGeometry(ids,capture,bounds,error),"recapture");const auto beforeCancel=session.UndoCount();session.BeginCoalescedEdit();target=bounds;target.x+=25;
 check(session.TransformGeometry(capture,bounds,target,error),"cancel preview");session.CancelCoalescedEdit();check(session.UndoCount()==beforeCancel&&session.FindWidget(ids[0])->rect.x==first.x,"cancel restores geometry/history");
 target=bounds;target.width=1;check(!session.TransformGeometry(capture,bounds,target,error)&&session.UndoCount()==beforeCancel,"minimum bounds atomic rejection");
 check(session.Save(error)&&session.Reload(error)&&session.FindWidget(ids[0])->rect.width>164.99f&&session.FindWidget(ids[0])->rect.width<165.01f,"batch save reload");
 // A group and an unrelated sibling scale from global captured bounds; its child follows exactly once.
 ScreenDocument grouped=doc;grouped.envelope.documentId=GenerateStableId();grouped.envelope.pathHint="Content/Screens/Groups.renegade-screen";
 auto group=MakeScreenWidgetPreset(ScreenWidgetPreset::Group,1280,720);group.id=GenerateStableId();group.rect={40,200,200,120};
 auto child=MakeScreenWidgetPreset(ScreenWidgetPreset::Panel,1280,720);child.id=GenerateStableId();child.parentId=group.id;child.rect={20,10,80,40};
 grouped.widgets.insert(grouped.widgets.end(),{group,child});const auto gp=root/grouped.envelope.pathHint;
 check(WriteScreenDocument(gp.generic_u8string(),grouped,error),"group fixture save");ScreenAuthoringSession groups;check(groups.Open(gp.generic_u8string(),project,error),"group fixture open");
 check(groups.CaptureGeometry({group.id,child.id,doc.widgets[0].id},capture,bounds,error),"mixed group roots capture");target=bounds;target.width*=2;target.height*=2;
 check(groups.TransformGeometry(capture,bounds,target,error),"mixed group transform");ScreenRect cr;check(ResolveScreenWidgetRect(groups.Document(),child.id,cr,error)&&cr.width==160&&cr.height==80,"group descendant scaled once");
 check(groups.UndoCount()==1&&groups.Undo(error)&&groups.FindWidget(child.id)->rect.width==80&&groups.Redo(error),"group batch single history step");
 check(groups.Save(error)&&groups.Reload(error)&&groups.FindWidget(child.id)->rect.width==160,"group reopen");
 grouped.widgets[grouped.widgets.size()-2].container.mode=ScreenChildLayout::Horizontal;
 check(WriteScreenDocument(gp.generic_u8string(),grouped,error)&&groups.Reload(error),"managed fixture reload");
 check(!groups.CaptureGeometry({child.id,doc.widgets[0].id},capture,bounds,error)&&groups.UndoCount()==0,"managed child batch accepted");
 ScreenAuthoringSession::GeometryCapture forged{{child.id,{60,210,80,40}}};
 check(!groups.TransformGeometry(forged,{60,210,80,40},{70,210,80,40},error)&&groups.UndoCount()==0,"batch API bypassed managed-child guard");
 // Thin authored dividers must still participate in batch moves.
 auto thin=doc;thin.envelope.documentId=GenerateStableId();thin.envelope.pathHint="Content/Screens/Thin.renegade-screen";
 auto divider=MakeScreenWidgetPreset(ScreenWidgetPreset::Divider,1280,720);divider.id=GenerateStableId();thin.widgets.push_back(divider);
 const auto tp=root/thin.envelope.pathHint;check(WriteScreenDocument(tp.generic_u8string(),thin,error),"thin fixture save");ScreenAuthoringSession thins;check(thins.Open(tp.generic_u8string(),project,error),"thin fixture open");
 check(thins.CaptureGeometry({divider.id,doc.widgets[0].id},capture,bounds,error),"thin selection capture");target=bounds;target.x+=10;
 check(thins.TransformGeometry(capture,bounds,target,error)&&thins.FindWidget(divider.id)->rect.height==2,"batch move regressed thin divider");
 // Picker contract uses distinct names/IDs and omits unavailable, disabled, standalone and current entries.
 std::vector<ScreenEditorLibraryScreen> entries;ScreenEditorLibraryScreen entry;entry.nodeId=GenerateStableId();entry.name="Options Display Name";entries.push_back(entry);
 entry.nodeId.clear();entries.push_back(entry);entry.nodeId=GenerateStableId();entry.available=false;entries.push_back(entry);entry.available=true;entry.enabledInGame=false;entries.push_back(entry);entry.enabledInGame=true;entry.current=true;entries.push_back(entry);
 const auto choices=ScreenDestinationChoices(entries);check(choices.size()==1&&choices[0].first=="Options Display Name"&&choices[0].second==entries[0].nodeId,"destination Choice label/value eligibility");
 // Cancellation after Undo preserves the divergent redo branch and the bounded history.
 check(session.CaptureGeometry(ids,capture,bounds,error),"prepare branch capture");target=bounds;target.x+=10;
 check(session.TransformGeometry(capture,bounds,target,error)&&session.Undo(error),"prepare gesture cancellation on redo branch");
 check(session.CaptureGeometry(ids,capture,bounds,error),"branch capture");const auto branchUndo=session.UndoCount(),branchRedo=session.RedoCount();
 session.BeginCoalescedEdit();target=bounds;target.x+=30;check(session.TransformGeometry(capture,bounds,target,error),"branch preview");session.CancelCoalescedEdit();
 check(session.UndoCount()==branchUndo&&session.RedoCount()==branchRedo&&session.Redo(error)&&session.FindWidget(ids[0])->rect.width>164.99f,"cancellation destroyed redo branch");
 // Durable project defaults, A/B overrides, foreign project rejection and failed-write rollback.
 std::ifstream geometryBefore(path,std::ios::binary);const std::string geometryBytes{std::istreambuf_iterator<char>(geometryBefore),{}};geometryBefore.close();
 ScreenGridPreferences prefs;check(ScreenGridPreferences::Load(root.generic_u8string(),project,prefs,error)&&prefs.defaults.spacing==16,"old project defaults");prefs.defaults={24,false,true};prefs.screens[doc.envelope.documentId]={32,true,false};
 check(prefs.Save(root.generic_u8string(),project,error),"grid save");ScreenGridPreferences reopened;
 check(ScreenGridPreferences::Load(root.generic_u8string(),project,reopened,error)&&reopened.Resolve(doc.envelope.documentId).spacing==32&&reopened.Resolve(GenerateStableId()).spacing==24,"A/B grid isolation reopen");
 auto fail=[](ProjectDocumentTransactionStage stage,std::size_t,const std::string&,std::string& e){if(stage==ProjectDocumentTransactionStage::AfterReplace){e="injected failure";return ProjectDocumentTransactionHookAction::Fail;}return ProjectDocumentTransactionHookAction::Continue;};
 prefs.defaults.spacing=64;check(!prefs.Save(root.generic_u8string(),project,error,fail),"failed grid save reported success");check(ScreenGridPreferences::Load(root.generic_u8string(),project,reopened,error)&&reopened.defaults.spacing==24,"failed write destroyed defaults");
 check(!ScreenGridPreferences::Load(root.generic_u8string(),GenerateStableId(),reopened,error),"foreign project grid accepted");
 prefs.screens.erase(doc.envelope.documentId);check(prefs.Save(root.generic_u8string(),project,error)&&ScreenGridPreferences::Load(root.generic_u8string(),project,reopened,error)&&reopened.Resolve(doc.envelope.documentId).spacing==64,"clear override inheritance");
 const auto projectB=GenerateStableId();const auto rootB=root/"B";std::filesystem::create_directories(rootB);
 ScreenGridPreferences prefsB;prefsB.defaults.spacing=8;check(prefsB.Save(rootB.generic_u8string(),projectB,error),"project B grid save");
 check(ScreenGridPreferences::Load(rootB.generic_u8string(),projectB,reopened,error)&&reopened.defaults.spacing==8&&
       ScreenGridPreferences::Load(root.generic_u8string(),project,reopened,error)&&reopened.defaults.spacing==64,"project A/B switching leaked defaults");
 std::ifstream geometryAfter(path,std::ios::binary);check(std::string{std::istreambuf_iterator<char>(geometryAfter),{}}==geometryBytes,"grid changed Screen geometry bytes");
 ScreenEditorWorkspacePreferences local;local.leftCollapsed=true;local.rightCollapsed=false;local.left=275;local.right=390;const auto workspace=root/"workspace.cfg";
 check(local.Save(workspace),"local workspace save");ScreenEditorWorkspacePreferences restored;check(ScreenEditorWorkspacePreferences::Load(workspace,restored)&&restored.leftCollapsed&&!restored.rightCollapsed&&restored.left==275&&restored.right==390,"independent collapse persistence");
 {std::ofstream old(workspace);old<<"renegade-ui-workspace-v1\n260 360 16 2 1 1\n";}
 check(ScreenEditorWorkspacePreferences::Load(workspace,restored)&&!restored.leftCollapsed&&!restored.rightCollapsed,"workspace v1 migration");
 std::cout<<"Authoring completion failures: "<<failures<<'\n';return failures?1:0;
}
