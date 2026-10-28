#include "StudioApplication.h"
#include "RiverNodeWidthGizmoAxis.h"
#include "renegade/bridge/RiverFlowRenderer.h"
#include <cmath>
#include "renegade/bridge/RiverCourseService.h"
#include "renegade/bridge/RiverWaterwayService.h"
namespace renegade::studio {
void StudioRenderPath::InitializeRiverFlowV2(){
 auto button=[&](SceneInspectorButton& b,const char* name,const char* text,int operation){
  b.Create(name);b.SetText(text);b.OnClick([this,operation](const wi::gui::EventArgs&){QueueRiverFlowV2(operation);});inspectorPanel_.AddWidget(&b);};
 // Simple first-class width control beside the native Scale gizmo.
 // It works at ANY selected control point; typing a precise width and
 // clicking RESET avoids relying on Undo to undo a difficult gizmo drag.
 riverNodeWidth_.Create(.25f,50.f,8.f,995,"Selected river node width",
    "SELECTED NODE WIDTH (m)");
 riverNodeWidth_.SetTooltip("Select a river control point. Width in metres at this point; the nearby channel and linked banks follow smoothly. RESET restores the whole-river base width. Undo/Redo supported.");
 riverNodeWidth_.OnValueCommitted([this](float metres){QueueRiverFlowV2(19,metres);});
 inspectorPanel_.AddWidget(&riverNodeWidth_);
 button(riverNodeWidthReset_,"Reset node river width","RESET SELECTED NODE WIDTH",20);
 riverNodeWidthReset_.SetTooltip("Set the selected river node back to the base river width (Scale X = 1), without using Undo.");
 button(riverFlowStartHere_,"Whitewater at selected river point",
    "START WHITEWATER AT THIS NODE",21);
 riverFlowStartHere_.SetTooltip("Select ANY blue river spline node, including a widened middle node. Foam starts at that node and travels downstream. Selecting the last node reverses a flat river so foam runs inward. Undoable.");
 button(riverFlowEnable_,"River V2 enable","ENABLE SPLINE FLOW + FOAM",0);
 button(riverFlowRapids_,"River V2 rapids","DEPTH-DRIVEN RAPIDS OFF",15);
 riverFlowRapids_.SetTooltip("Additional measured-bed rapids. Elevated rivers create smooth drops automatically; this control adds shallow-bed emission. Requires a continuous static bed within four metres.");
 button(riverFlowFoam_,"River V2 foam","CONTACT FOAM ON",1);
 button(riverFlowStartFoam_,"River source whitewater","START WHITEWATER OFF",17);
 riverFlowStartFoam_.SetTooltip("Optional foam at the upstream start of this standalone river. Use beneath a waterfall or at naturally turbulent river sources. Foam travels and fades downstream; no separate pool.");
 button(riverFlowReverse_,"River V2 reverse","REVERSE FLOW",2);
 button(riverFlowReset_,"River V2 reset","RESET FLOW SETTINGS",3);
 button(riverDrawTributary_,"Draw tributary","DRAW TRIBUTARY FROM THIS NODE",14);
 riverDrawTributary_.SetTooltip("Select an interior receiving river control point. Click Draw tributary, then drag from the highlighted node outward across terrain. Release creates the incoming river, connection and riverbed as one Undo. Right-click during a stroke cancels.");
 button(riverConnect_,"River connection","CONNECT EXISTING TRIBUTARY",11);
 button(riverDisconnect_,"River disconnect","DISCONNECT TRIBUTARY",12);
 button(riverConnectCancel_,"Cancel river connection","CANCEL CONNECTION",13);
 riverConnect_.SetTooltip("Select the tributary, click Connect, then select an existing control point on the receiving river and confirm. Maximum snap distance 1 metre; one flat confluence.");
 riverFlowArrows_.Create("River flow arrows");riverFlowArrows_.SetText("FLOW ARROWS OFF");
 riverFlowArrows_.OnClick([this](const wi::gui::EventArgs&){riverFlowArrowsVisible_=!riverFlowArrowsVisible_;riverFlowArrows_.SetText(riverFlowArrowsVisible_?"FLOW ARROWS ON":"FLOW ARROWS OFF");});inspectorPanel_.AddWidget(&riverFlowArrows_);
 const float low[]={0,0,.5f,0,.01f,-.15f,.5f,0,0},
             high[]={3,2,6,1.5f,.08f,.15f,50,2,2},
             defaults[]={1,1,3.2f,1,.04f,0,4,1,.65f};
 const char* names[]={"Flow speed (m/s)","Foam intensity","Foam lifespan (s)","Wake turbulence","Contact width (m)","Foam breakup threshold","Base river width (m)","Rapids foam amount","Start whitewater amount"};
 for(size_t k=0;k<riverFlowSliders_.size();++k){auto& slider=riverFlowSliders_[k];slider.Create(low[k],high[k],defaults[k],300,names[k],names[k]);
  slider.OnValueCommitted([this,k](float v){QueueRiverFlowV2(k==8?18:(k==7?16:int(k)+4),v);});inspectorPanel_.AddWidget(&slider);}
 riverFlowSliders_[0].SetTooltip("Selected river inlet speed. Connected downstream flux is rebalanced from both inlets. Rebuilds transport once on commit; zero-speed connections are unsupported.");
 riverFlowSliders_[3].SetTooltip("Elevated rivers: strength of transported surface normal turbulence. Connected flat rivers: unavailable.");
 riverFlowSliders_[4].SetTooltip("Shared rock-contact foam emission falloff in metres. Elevated course: rebuilds transport on commit.");
 riverFlowSliders_[7].SetTooltip("Scales only physically gated rapid emission; zero preserves bed-driven flow and contact foam. Existing foam decays naturally.");
 riverFlowSliders_[8].SetTooltip("Foam emitted at the start, then carried downstream by existing G11 flow. No new water surface. Set to 0 for no impact foam.");
 riverFlowSliders_[6].SetTooltip("Base ribbon width before control-point and parent scaling. Linked riverbeds update with the river.");
 riverFlowStatus_.Create("River flow status");riverFlowStatus_.SetText("Native water | flow disabled");inspectorPanel_.AddWidget(&riverFlowStatus_);
}
void StudioRenderPath::SyncRiverFlowV2(wi::ecs::Entity river){
 bridge::RiverFlowProfile p;std::string error;bridge::RiverWaterwayControls controls;
 const bool elevated=session_&&bridge::RiverCourseHasElevation(session_->Scenes().GetScene(),river);
 bool valid=session_&&bridge::ReadRiverWaterwayControls(session_->Scenes().GetScene(),river,controls,error)&&bridge::ReadRiverFlowProfile(session_->Scenes().GetScene(),controls.owner,p,error);
 for(auto* w:RiverFlowV2Widgets())w->SetVisible(false);
 riverFlowEnable_.SetEnabled(valid);riverFlowEnable_.SetText(p.enabled?"DISABLE SPLINE FLOW + FOAM":"ENABLE SPLINE FLOW + FOAM");
 riverFlowRapids_.SetText(p.rapids?"DEPTH-DRIVEN RAPIDS ON":"DEPTH-DRIVEN RAPIDS OFF");riverFlowRapids_.SetEnabled(valid&&(controls.connected||elevated)&&p.enabled);
 riverFlowStartFoam_.SetText(p.startWhitewater?"START WHITEWATER ON":"START WHITEWATER OFF");
 riverFlowStartFoam_.SetEnabled(valid&&!controls.connected);
 riverFlowFoam_.SetText(p.foam?"CONTACT FOAM ON":"CONTACT FOAM OFF");
 riverFlowReverse_.SetText(p.reversed?"FLOW REVERSED // SWITCH FORWARD":"FLOW FORWARD // REVERSE");
 riverFlowArrows_.SetText(riverFlowArrowsVisible_?"FLOW ARROWS ON":"FLOW ARROWS OFF");
 const auto* spline=session_?session_->Scenes().GetScene().splines.GetComponent(river):nullptr;
 const auto selectedNode=session_?session_->Selection().SelectedEntity():wi::ecs::INVALID_ENTITY;
 const auto nodeIt=spline?std::find(spline->spline_node_entities.begin(),
     spline->spline_node_entities.end(),selectedNode):std::vector<wi::ecs::Entity>::const_iterator{};
 const bool hasNode=spline&&nodeIt!=spline->spline_node_entities.end();
 const bool selectedEnd=hasNode&&(nodeIt==spline->spline_node_entities.begin()||
    nodeIt+1==spline->spline_node_entities.end());
 const auto* nodePose=hasNode?session_->Scenes().GetScene().transforms.GetComponent(selectedNode):nullptr;
 const float baseNodeWidth=spline?spline->width*2.f:4.f;
 const float exactNodeWidth=nodePose?baseNodeWidth*nodePose->scale_local.x:baseNodeWidth;
 riverNodeWidth_.SetText(!nodePose?"SELECT A RIVER NODE TO EDIT LOCAL WIDTH":
    detail::RiverNodeWidthUsesZHandle(session_->Scenes().GetScene(),selectedNode)
      ?"NODE WIDTH (m) // SCALE Z HANDLE":"NODE WIDTH (m) // SCALE X HANDLE");
 riverNodeWidth_.SetRange(std::max(.25f,baseNodeWidth*.125f),
     std::min(50.f,baseNodeWidth*4.f));
 riverNodeWidth_.SetEnabled(valid&&!controls.connected&&nodePose);
 riverNodeWidthReset_.SetEnabled(valid&&!controls.connected&&nodePose&&
     std::abs(nodePose->scale_local.x-1.f)>.0001f);
 riverFlowStartHere_.SetEnabled(valid&&!controls.connected&&!elevated&&hasNode);
 const int selectedIndex=hasNode?int(nodeIt-spline->spline_node_entities.begin()):-2;
 const bool whitewaterHere=p.startWhitewater&&hasNode&&
    (!p.startWhitewaterNodeId.empty()
      ?bridge::PersistentEntityId(session_->Scenes().GetScene(),selectedNode)==p.startWhitewaterNodeId
      :((p.startWhitewaterNodeIndex==selectedIndex&&selectedIndex>=0)||
        (p.startWhitewaterNodeIndex==-1&&
          ((selectedIndex==0&&!p.reversed)||
            (selectedEnd&&nodeIt+1==spline->spline_node_entities.end()&&p.reversed)))));
 riverFlowStartHere_.SetText(!hasNode?"SELECT A RIVER NODE FOR WHITEWATER":
     whitewaterHere?"WHITEWATER STARTS AT THIS NODE":"START WHITEWATER AT THIS NODE");
 if(!riverNodeWidth_.IsDragging())riverNodeWidth_.SetValue(exactNodeWidth);
 bridge::RiverFlowProfile inlet=p;if(valid&&controls.connected)bridge::ReadRiverFlowProfile(session_->Scenes().GetScene(),river,inlet,error);
 riverFlowSliders_[0].SetRange(controls.connected?.05f:0.f,3.f);
 riverFlowSliders_[0].SetText(controls.connected?"Inlet speed (m/s)":"Flow speed (m/s)");
 const float values[]={inlet.speed,p.intensity,p.lifetime,p.turbulence,p.sourceWidth,p.threshold,spline?spline->width*2:4,p.rapidIntensity,p.startWhitewaterAmount};
 for(size_t k=0;k<riverFlowSliders_.size();++k){auto& slider=riverFlowSliders_[k];slider.SetEnabled(valid&&p.enabled);if(!slider.IsDragging())slider.SetValue(values[k]);}
 // Base width edits remain available when hydraulic flow is switched off.
 riverFlowSliders_[6].SetEnabled(valid&&!controls.connected);
 riverFlowSliders_[7].SetEnabled(valid&&(controls.connected||elevated)&&p.enabled&&(p.rapids||elevated));
 riverFlowSliders_[8].SetEnabled(valid&&!controls.connected&&p.enabled&&p.foam&&p.startWhitewater);
 riverFlowSliders_[3].SetText(elevated?"Surface turbulence":"Wake turbulence");
 // Shape/direction edits need coordinated graph validation. Do not expose a
 // control that can silently break a saved connection or has no shared effect.
 riverFlowFoam_.SetEnabled(valid);riverFlowReset_.SetEnabled(valid&&!controls.connected);
 riverFlowReverse_.SetEnabled(valid&&!controls.connected&&!elevated);
 if(controls.connected){
  for(size_t k:{size_t(3),size_t(6)})riverFlowSliders_[k].SetEnabled(false);
 }
 if(session_&&!riverDrawingReceiver_.empty()&&(!terrainRiverBrushActive_||riverDrawingSceneRevision_!=session_->Scenes().Revision())){
  riverDrawingReceiver_.clear();riverDrawingControl_.clear();terrainRiverBrushActive_=false;terrainRiverStrokeActive_=false;terrainRiverStrokePoints_.clear();
 }
 bool interior=false;
 if(spline&&session_){const auto node=session_->Selection().SelectedEntity();auto it=std::find(spline->spline_node_entities.begin(),spline->spline_node_entities.end(),node);
  interior=it!=spline->spline_node_entities.end()&&it!=spline->spline_node_entities.begin()&&it+1!=spline->spline_node_entities.end();}
 riverDrawTributary_.SetEnabled(!riverDrawingReceiver_.empty()||(valid&&!controls.connected&&interior&&session_->Scenes().GetScene().terrains.GetCount()==1));
 riverDrawTributary_.SetText(riverDrawingReceiver_.empty()?"DRAW TRIBUTARY FROM THIS NODE":"CANCEL TRIBUTARY DRAWING");
 if(session_&&riverConnectionSceneRevision_!=session_->Scenes().Revision())riverConnectionSource_.clear();
 riverConnect_.SetEnabled(valid);riverConnect_.SetText(riverConnectionSource_.empty()?"CONNECT EXISTING TRIBUTARY":"CONFIRM RECEIVING NODE");
 riverConnectCancel_.SetEnabled(!riverConnectionSource_.empty());
 bool attached=false;if(session_){const auto* metadata=session_->Scenes().GetScene().metadatas.GetComponent(river);attached=metadata&&metadata->string_values.has(bridge::RiverConnectionMetadataKey);}
 riverDisconnect_.SetEnabled(attached);
 std::string status=valid?riverFlow_.Status(river):error;
 if(valid&&controls.connected){
  const auto& scene=session_->Scenes().GetScene();
  auto name=[&](wi::ecs::Entity e,const char* fallback){const auto* n=scene.names.GetComponent(e);return n&&!n->name.empty()?n->name:std::string(fallback);};
  status="Shared foam: "+name(controls.owner,"receiving river")+" / "+name(controls.receivingNode,"receiving node")+" | "+status;
 }
 if(!riverDrawingReceiver_.empty())status="DRAW TRIBUTARY: drag outward from the highlighted node; release to finish";
 if(session_&&spline){
  const auto selected=session_->Selection().SelectedEntity();
  if(std::find(spline->spline_node_entities.begin(),
       spline->spline_node_entities.end(),selected)!=spline->spline_node_entities.end()){
   const auto* node=session_->Scenes().GetScene().transforms.GetComponent(selected);
   if(node)
    status=std::string("SELECTED RIVER NODE // SCALE ")+
      (detail::RiverNodeWidthUsesZHandle(session_->Scenes().GetScene(),selected)?"Z":"X")+
      " across river to change width ("+
      std::to_string(spline->width*2.f*node->scale_local.x)+
      " m) // or use NODE WIDTH slider above // Undo supported | "+status;
  }
 }
 riverFlowStatus_.SetText(!riverConnectionSource_.empty()?"CONNECTION: select an existing node on the receiving river, then confirm":status);
}
std::vector<wi::gui::Widget*> StudioRenderPath::RiverFlowV2Widgets(){
 std::vector<wi::gui::Widget*> out={&riverNodeWidth_,&riverNodeWidthReset_,&riverFlowStartHere_,&riverFlowRapids_,&riverFlowStartFoam_,&riverDrawTributary_,&riverFlowEnable_,&riverFlowArrows_,&riverFlowStatus_,&riverFlowFoam_,&riverFlowReverse_,&riverFlowReset_,&riverConnect_,&riverDisconnect_,&riverConnectCancel_};
 for(auto& slider:riverFlowSliders_)out.push_back(&slider);return out;
}
void StudioRenderPath::QueueRiverFlowV2(int operation,float value){
 if(!session_||projectHubVisible_||testLevelRuntime_.IsActive())return;
 auto& scene=session_->Scenes().GetScene();auto river=session_->Selection().SelectedEntity();
 if(!scene.splines.Contains(river))for(size_t k=0;k<scene.splines.GetCount();++k)
  if(scene.Entity_IsDescendant(river,scene.splines.GetEntity(k))){river=scene.splines.GetEntity(k);break;}
 const auto selected=session_->Selection().SelectedEntity();
 const auto armed=riverConnectionSource_;
 const auto revision=session_->Scenes().Revision();
 wi::eventhandler::Subscribe_Once(wi::eventhandler::EVENT_THREAD_SAFE_POINT,[this,river,selected,armed,revision,operation,value](uint64_t){
  if(!session_||testLevelRuntime_.IsActive()||session_->Scenes().Revision()!=revision)return;
  auto& scene=session_->Scenes().GetScene();bridge::RiverFlowProfile p;std::string error;
  if(!bridge::ReadRiverFlowProfile(scene,river,p,error)){studioChrome_.SetStatusText(error);return;}
  if(operation==19 || operation==20 || operation==21){
   const auto* spline=scene.splines.GetComponent(river);
   if(!spline){studioChrome_.SetStatusText("RIVER // Select a spline node first");return;}
   const auto it=std::find(spline->spline_node_entities.begin(),
      spline->spline_node_entities.end(),selected);
   if(it==spline->spline_node_entities.end()){
    studioChrome_.SetStatusText("RIVER // Select a blue spline node first");return;
   }
   bridge::RiverWaterwayControls sourceControls;std::string sourceError;
   if(!bridge::ReadRiverWaterwayControls(scene,river,sourceControls,sourceError)||
      sourceControls.connected){
    studioChrome_.SetStatusText("RIVER // Disconnect tributary before editing source or width");return;
   }
   if(operation==19 || operation==20){
    auto* pose=scene.transforms.GetComponent(selected);
    if(!pose||!std::isfinite(spline->width)||spline->width<=0)return;
    const float base=spline->width*2.f;
    const float scale=operation==20?1.f:std::clamp(value/base,.125f,4.f);
    const auto before=bridge::CaptureTransform(*pose);
    auto after=before;after.scale.x=scale;after.scale.y=before.scale.y;after.scale.z=before.scale.z;
    if(std::abs(before.scale.x-scale)<.0001f)return;
    if(!session_->Commands().Execute(std::make_unique<bridge::SetWatercourseTransformCommand>(
        scene,selected,before,after))){
      studioChrome_.SetStatusText("RIVER WIDTH // This width would break the native banks; no changes applied");
    }else{
      studioChrome_.SetStatusText("RIVER WIDTH // Node now "+std::to_string(base*scale)+
        "m // Reset or drag slider back at any time");
    }
   }else{
    const bool first=it==spline->spline_node_entities.begin();
    const bool last=it+1==spline->spline_node_entities.end();
    const int index=int(it-spline->spline_node_entities.begin());
    if(bridge::RiverCourseHasElevation(scene,river)){
     studioChrome_.SetStatusText("WHITEWATER // Reversing a graded course is not supported");return;
    }
    bridge::RiverFlowProfile head;std::string error;
    if(!bridge::ReadRiverFlowProfile(scene,river,head,error)){
     studioChrome_.SetStatusText(error);return;
    }
    if(first)head.reversed=false;
    if(last)head.reversed=true;
    head.startWhitewaterNodeIndex=(first||last)?-1:index;
    head.startWhitewaterNodeId.clear(); // switch to head or resolve selected node on commit
    head.enabled=true;head.foam=true;head.startWhitewater=true;
    if(!session_->Commands().Execute(std::make_unique<bridge::SetRiverFlowProfileCommand>(
        scene,river,head))){
      studioChrome_.SetStatusText("WHITEWATER // Source cannot be changed here");return;
    }
    studioChrome_.SetStatusText("WHITEWATER // Starts at selected river node "+
       std::to_string(index+1)+" and flows downstream");
   }
   QueueInspectorRefresh();RefreshStatus();return;
  }
  if(operation==14){
   if(!riverDrawingReceiver_.empty()){riverDrawingReceiver_.clear();riverDrawingControl_.clear();terrainRiverBrushActive_=false;terrainRiverStrokeActive_=false;terrainRiverStrokePoints_.clear();terrainRiverBrushButton_.SetText("DRAW RIVER");QueueInspectorRefresh();return;}
   bridge::RiverWaterwayControls controls;DirectX::XMFLOAT3 anchor;
   const auto* spline=scene.splines.GetComponent(river);
   const auto it=spline?std::find(spline->spline_node_entities.begin(),spline->spline_node_entities.end(),selected):decltype(spline->spline_node_entities.begin()){};
   if(!spline||it==spline->spline_node_entities.end()||it==spline->spline_node_entities.begin()||it+1==spline->spline_node_entities.end()||
      scene.terrains.GetCount()!=1||!bridge::ReadRiverWaterwayControls(scene,river,controls,error)||controls.connected||
      !bridge::RiverConnectionAnchor(scene,river,selected,anchor,error)){
    studioChrome_.SetStatusText("DRAW TRIBUTARY // Select an interior control point on an unconnected flat river");return;
   }
   riverDrawingReceiver_=bridge::PersistentEntityId(scene,river);riverDrawingControl_=bridge::PersistentEntityId(scene,selected);
   riverDrawingAnchor_=anchor;riverDrawingSceneRevision_=revision;riverConnectionSource_.clear();
   DisableWd01VegetationBrush();terrainRiverBrushActive_=true;terrainRiverStrokeActive_=false;terrainRiverStrokePoints_.clear();
   terrainRiverWidthValue_=std::clamp(spline->width*1.2f,1.f,50.f);terrainRiverWidth_.SetValue(terrainRiverWidthValue_);
   terrainRiverBrushButton_.SetText("STOP RIVER");QueueInspectorRefresh();RefreshStatus();
   studioChrome_.SetStatusText("DRAW TRIBUTARY // Drag from the highlighted river node outward, then release; right-click during drawing cancels");return;
  }
  if(operation>=11&&operation<=13){
   std::string feedback;
   if(operation==13){riverConnectionSource_.clear();feedback="RIVER CONNECTION // Cancelled; scene unchanged";}
   else if(operation==12){if(!session_->Commands().Execute(bridge::MakeDisconnectRiverCommand(scene,river)))feedback="RIVER CONNECTION // Disconnect refused; scene unchanged";}
   else if(armed.empty()){riverConnectionSource_=bridge::PersistentEntityId(scene,river);riverConnectionSceneRevision_=revision;feedback="RIVER CONNECTION // Select a receiving river control point, then confirm";}
   else {
    bridge::EntityIdentityIndex identities;
    if(riverConnectionSceneRevision_!=revision||!identities.Build(scene,error)){riverConnectionSource_.clear();feedback="RIVER CONNECTION // Scene changed; select the tributary again";}
    else if(!session_->Commands().Execute(bridge::MakeSetRiverConnectionCommand(scene,identities.Resolve(armed),river,selected)))feedback="RIVER CONNECTION // Choose a receiving node within 1 metre at the same water level; one flat confluence is supported";
    else {riverConnectionSource_.clear();feedback="RIVER CONNECTION // Tributary connected; Undo restores the original spline and bed";}
   }
   QueueInspectorRefresh();RefreshStatus();if(!feedback.empty())studioChrome_.SetStatusText(feedback);return;
  }
  bridge::RiverWaterwayControls controls;
  if(!bridge::ReadRiverWaterwayControls(scene,river,controls,error)){studioChrome_.SetStatusText(error);return;}
  if(controls.connected&&(operation==2||operation==3||operation==7||operation==10||operation==17||operation==18)){
   studioChrome_.SetStatusText("CONNECTED RIVER // Disconnect before changing width or direction; wake turbulence is not supported yet");return;
  }
  const auto profileRiver=operation==4?river:controls.owner;
  if(!bridge::ReadRiverFlowProfile(scene,profileRiver,p,error)){studioChrome_.SetStatusText(error);return;}
  if(operation==10){
   if(!session_->Commands().Execute(bridge::MakeSetRiverWidthCommand(scene,river,value)))studioChrome_.SetStatusText("RIVER WIDTH // Unsafe width or linked riverbed change refused");
  }else{
   if(operation==0)p.enabled=!p.enabled;
   else if(operation==1)p.foam=!p.foam;
   else if(operation==2)p.reversed=!p.reversed;
   else if(operation==3){p={};p.enabled=true;}
   else if(operation==4)p.speed=value;
   else if(operation==5)p.intensity=value;
   else if(operation==6)p.lifetime=value;
   else if(operation==7)p.turbulence=value;
   else if(operation==8)p.sourceWidth=value;
   else if(operation==9)p.threshold=value;
   else if(operation==15)p.rapids=!p.rapids;
   else if(operation==16)p.rapidIntensity=value;
   else if(operation==17){
    p.startWhitewater=!p.startWhitewater;
    // One creator click must actually produce whitewater. Do not require
    // understanding the separate G11 flow simulation enable toggle first.
    if(p.startWhitewater){p.enabled=true;p.foam=true;}
   }
   else if(operation==18)p.startWhitewaterAmount=value;
   if(!session_->Commands().Execute(std::make_unique<bridge::SetRiverFlowProfileCommand>(scene,profileRiver,p)))
    studioChrome_.SetStatusText("RIVER FLOW // Settings refused; scene unchanged");
  }
  QueueInspectorRefresh();RefreshStatus();
 });
}
}
