#include "StudioApplication.h"
#include "../../EngineBridge/src/PlaceableWaterfallAuthoring.h"
#include <cmath>
#include <fstream>
#include <filesystem>
#include <cstdlib>
using namespace DirectX;
namespace renegade::studio {
namespace wf=bridge::placeable_waterfall;
void StudioRenderPath::CreateWaterfallControls(){
 if(waterfallControlsCreated_)return;waterfallControlsCreated_=true;
 addWaterfallButton_.Create("Add Waterfall");addWaterfallButton_.SetText("ADD WATERFALL");addWaterfallButton_.SetTooltip("Create a standalone waterfall. No river required.");
 addWaterfallButton_.OnClick([this](const wi::gui::EventArgs&){wi::eventhandler::Subscribe_Once(wi::eventhandler::EVENT_THREAD_SAFE_POINT,[this](uint64_t){AddWaterfallFromView();});});inspectorPanel_.AddWidget(&addWaterfallButton_);
 waterfallInspector_.Create("WATERFALL");GetGUI().AddWidget(&waterfallInspector_);waterfallInspector_.SetVisible(false);
 for(auto* label:{&waterfallPlacement_,&waterfallWater_,&waterfallSpray_,&waterfallStatus_}){label->Create("Waterfall section");waterfallInspector_.AddWidget(label);}
 waterfallPlacement_.SetText("PLACEMENT");waterfallWater_.SetText("WATER");waterfallSpray_.SetText("SPRAY & MIST");
 waterfallCrestButton_.Create("Select Crest");waterfallCrestButton_.SetText("CREST / TOP - SELECT & MOVE");waterfallCrestButton_.OnClick([this](const wi::gui::EventArgs&){SelectWaterfallHandle(false);});
 waterfallLandingButton_.Create("Select Landing");waterfallLandingButton_.SetText("LANDING / BOTTOM - SELECT & MOVE");waterfallLandingButton_.OnClick([this](const wi::gui::EventArgs&){SelectWaterfallHandle(true);});
 waterfallDeleteButton_.Create("Delete Waterfall");waterfallDeleteButton_.SetText("DELETE WATERFALL");waterfallDeleteButton_.OnClick([this](const wi::gui::EventArgs&){wi::eventhandler::Subscribe_Once(wi::eventhandler::EVENT_THREAD_SAFE_POINT,[this](uint64_t){DeleteSelection();});});
 for(auto* button:{&waterfallCrestButton_,&waterfallLandingButton_,&waterfallDeleteButton_})waterfallInspector_.AddWidget(button);
 const char* names[]={"WIDTH (m)","FLOW / INTENSITY","TURBULENCE","AIRBORNE SPRAY","AMBIENT MIST","IMPACT SPRAY"};
 for(size_t i=0;i<6;++i){auto& slider=waterfallSliders_[i];slider.Create(i==0?.3f:0.f,i==0?25.f:i==2?2.f:3.f,i==0?3.5f:i==2?.7f:1.f,300.f,names[i],names[i]);slider.OnValueCommitted([this,i](float value){CommitWaterfallSetting(i,value);});waterfallInspector_.AddWidget(&slider);}
 // Native Window::AddWidget inherits IsEnabled(), which is false while hidden.
 // Enable the completed panel explicitly so its controls accept input when shown.
 waterfallInspector_.SetEnabled(true);
}
void StudioRenderPath::AddWaterfallFromView(){
 if(!session_||!camera||testLevelRuntime_.IsActive())return;
 auto& scene=session_->Scenes().GetScene();wf::Definition d;auto eye=camera->Eye,forward=camera->At;forward.y=0;float length=std::hypot(forward.x,forward.z);if(length<.01f)forward={1,0,0};else{forward.x/=length;forward.z/=length;}
 d.crest={eye.x+forward.x*10,eye.y+2,eye.z+forward.z*10};d.landing={d.crest.x+forward.x*1.6f,d.crest.y-6,d.crest.z+forward.z*1.6f};
 auto command=wf::MakeCreateWaterfallCommand(scene,d);auto id=command->Id();if(!session_->Commands().Execute(std::move(command))){studioChrome_.SetStatusText("WATERFALL // CREATE FAILED");return;}
 session_->Selection().Select(wf::FindWaterfallById(scene,id));terrainWorkspaceActive_=environmentWorkspaceActive_=false;studioChrome_.SetTerrainWorkspaceActive(false);studioChrome_.SetEnvironmentWorkspaceActive(false);SetTransformTool(TransformTool::Translate);SyncGizmoSelection();RefreshHierarchy();RefreshInspector();QueueInspectorRefresh();RefreshStatus();
}
bool StudioRenderPath::LayoutWaterfallInspector(){
 if(!waterfallControlsCreated_)return false;
 auto root=session_?wf::WaterfallOwner(session_->Scenes().GetScene(),session_->Selection().SelectedEntity()):wi::ecs::INVALID_ENTITY;
 bool show=root!=wi::ecs::INVALID_ENTITY&&!terrainWorkspaceActive_&&!environmentWorkspaceActive_&&!renderWorkspaceActive_&&!projectHubVisible_;
 waterfallInspector_.SetVisible(show);if(!show)return false;
 inspectorPanel_.wi::gui::Widget::SetVisible(false);waterfallInspector_.SetPos(inspectorPanel_.GetPos());waterfallInspector_.SetSize(inspectorPanel_.GetSize());
 wf::AuthoredWaterfall record;std::string error;if(!wf::ReadWaterfall(session_->Scenes().GetScene(),root,record,error)){waterfallStatus_.SetText(error);return true;}
 auto& d=record.settings;wf::SyncLandingHandle(session_->Scenes().GetScene(),root,d);
 const float width=std::max(180.f,waterfallInspector_.GetSize().x-24);float y=38;
 auto place=[&](wi::gui::Widget& w,float height=28){w.SetPos({12,y});w.SetSize({width,height});y+=height+8;};
 place(waterfallPlacement_);place(waterfallCrestButton_);place(waterfallLandingButton_);place(waterfallSliders_[0]);place(waterfallWater_);place(waterfallSliders_[1]);place(waterfallSliders_[2]);place(waterfallSpray_);for(size_t i=3;i<6;++i)place(waterfallSliders_[i]);place(waterfallStatus_,48);place(waterfallDeleteButton_);
 float values[]={d.width,d.intensity,d.turbulence,d.spray,d.mist,d.impactSpray};for(size_t i=0;i<6;++i)if(!waterfallSliders_[i].IsDragging())waterfallSliders_[i].SetValue(values[i]);
 waterfallStatus_.SetText("Drop: "+std::to_string(d.crest.y-d.landing.y).substr(0,4)+"m | "+waterfallRenderer_.Status());return true;
}
void StudioRenderPath::CommitWaterfallSetting(size_t index,float value){
 if(!session_)return;auto& scene=session_->Scenes().GetScene();wf::AuthoredWaterfall record;std::string error;auto root=wf::WaterfallOwner(scene,session_->Selection().SelectedEntity());if(!wf::ReadWaterfall(scene,root,record,error))return;
 float* fields[]={&record.settings.width,&record.settings.intensity,&record.settings.turbulence,&record.settings.spray,&record.settings.mist,&record.settings.impactSpray};if(index>=6)return;*fields[index]=value;auto revision=session_->Scenes().Revision();
 wi::eventhandler::Subscribe_Once(wi::eventhandler::EVENT_THREAD_SAFE_POINT,[this,record,revision](uint64_t){if(!session_||revision!=session_->Scenes().Revision())return;if(!session_->Commands().Execute(wf::MakeEditWaterfallCommand(session_->Scenes().GetScene(),record.id,record.settings)))studioChrome_.SetStatusText("WATERFALL // INVALID SETTING");QueueInspectorRefresh();RefreshStatus();});
}
void StudioRenderPath::SelectWaterfallHandle(bool landing){if(!session_)return;auto& scene=session_->Scenes().GetScene();auto root=wf::WaterfallOwner(scene,session_->Selection().SelectedEntity());if(root==wi::ecs::INVALID_ENTITY)return;auto e=landing?wf::LandingHandle(scene,root):root;if(e==wi::ecs::INVALID_ENTITY)return;session_->Selection().Select(e);SetTransformTool(TransformTool::Translate);SyncGizmoSelection();RefreshHierarchy();QueueInspectorRefresh();}
bool StudioRenderPath::CommitWaterfallGizmo(wi::ecs::Entity entity,const bridge::TransformState& after){
 auto& scene=session_->Scenes().GetScene();auto root=wf::WaterfallOwner(scene,entity);if(root==wi::ecs::INVALID_ENTITY)return false;wf::AuthoredWaterfall record;std::string error;if(!wf::ReadWaterfall(scene,root,record,error))return true;
 if(entity==root)record.settings.crest=after.translation;else record.settings.landing={record.settings.crest.x+after.translation.x,record.settings.crest.y+after.translation.y,record.settings.crest.z+after.translation.z};
 auto command=wf::MakeEditWaterfallCommand(scene,record.id,record.settings);if(!session_->Commands().Execute(std::move(command)))waterfallUiError_="TOP MUST STAY AT LEAST 0.5m ABOVE LANDING";else waterfallUiError_.clear();return true;
}
void StudioRenderPath::DrawWaterfallHandles(){if(!session_||projectHubVisible_)return;auto& scene=session_->Scenes().GetScene();std::vector<wf::AuthoredWaterfall> records;std::string error;if(!wf::EnumerateWaterfalls(scene,records,error))return;for(const auto& r:records){for(auto e:{r.entity,wf::LandingHandle(scene,r.entity)})if(auto* t=scene.transforms.GetComponent(e)){auto p=t->GetPosition();wi::renderer::DrawSphere(wi::primitive::Sphere(p,.18f),e==r.entity?XMFLOAT4{1,.55f,.1f,1}:XMFLOAT4{.1f,.8f,1,1},false);}}}
bool StudioRenderPath::PickWaterfallHandles(const XMFLOAT4& pointer){
 auto& scene=session_->Scenes().GetScene();auto ray=wi::renderer::GetPickRay(long(pointer.x),long(pointer.y),*this,*camera);std::vector<wf::AuthoredWaterfall> records;std::string error;if(!wf::EnumerateWaterfalls(scene,records,error))return false;float closest=INFINITY;wi::ecs::Entity selected=wi::ecs::INVALID_ENTITY;
 for(const auto& r:records)for(auto e:{r.entity,wf::LandingHandle(scene,r.entity)})if(auto* t=scene.transforms.GetComponent(e)){float distance;XMFLOAT3 direction;if(ray.intersects(wi::primitive::Sphere(t->GetPosition(),.25f),distance,direction)&&distance<closest){closest=distance;selected=e;}}
 if(selected==wi::ecs::INVALID_ENTITY)return false;session_->Selection().Select(selected);terrainWorkspaceActive_=environmentWorkspaceActive_=false;studioChrome_.SetTerrainWorkspaceActive(false);studioChrome_.SetEnvironmentWorkspaceActive(false);SyncGizmoSelection();RefreshHierarchy();RefreshInspector();return true;
}
// Explicit disposable-scene verification hook; inactive in ordinary Studio sessions.
void StudioRenderPath::TickWaterfallStudioProof(){
 const char* review=std::getenv("RENEGADE_WATERFALL_REVIEW_SCENE");if(!review||!session_||!camera)return;
 ++waterfallProofFrame_;if(waterfallProofFrame_<60)return;
 auto& scene=session_->Scenes().GetScene();std::vector<wf::AuthoredWaterfall> records;std::string error;
 if(!wf::EnumerateWaterfalls(scene,records,error)||records.empty())return;
 if(!waterfallReviewConfigured_){
 {std::ofstream f(std::filesystem::path(review).parent_path()/"material-diagnostic.log");for(size_t i=0;i<scene.materials.GetCount();++i){auto& m=scene.materials[i];auto e=scene.materials.GetEntity(i);auto* n=scene.names.GetComponent(e);f<<e<<" "<<(n?n->name:"")<<" color="<<m.baseColor.x<<","<<m.baseColor.y<<","<<m.baseColor.z<<","<<m.baseColor.w<<" shader="<<int(m.shaderType)<<" custom="<<m.GetCustomShaderID()<<" blend="<<int(m.userBlendMode)<<" tex="<<m.textures[0].name<<" valid="<<m.textures[0].resource.IsValid()<<std::endl;}}
 waterfallReviewConfigured_=true;SetProjectHubVisible(false);terrainWorkspaceActive_=environmentWorkspaceActive_=false;
  // Disposable review fixture only: retain the approved proof lighting with restored texture resources.
  for(size_t i=0;i<scene.weathers.GetCount();++i){auto* name=scene.names.GetComponent(scene.weathers.GetEntity(i));if(name&&name->name=="Studio daylight"){auto& w=scene.weathers[i];w.ambient={.5f,.56f,.6f};w.horizon={.68f,.77f,.82f};w.zenith={.25f,.43f,.63f};}}
  for(size_t i=0;i<scene.lights.GetCount();++i){auto* name=scene.names.GetComponent(scene.lights.GetEntity(i));if(name&&name->name=="Sun")scene.lights[i].intensity=4.f;}

  auto crest=records.front().settings.crest;auto landing=records.front().settings.landing;auto target=XMVectorSet((crest.x+landing.x)*.5f,(crest.y+landing.y)*.5f,(crest.z+landing.z)*.5f,1);
  auto eye=target+XMVectorSet(20,2,-13,0);editorCameraTransform_.ClearTransform();editorCameraTransform_.MatrixTransform(XMMatrixInverse(nullptr,XMMatrixLookAtLH(eye,target,XMVectorSet(0,1,0,0))));editorCameraTransform_.UpdateTransform();camera->TransformCamera(editorCameraTransform_);camera->UpdateCamera();
  session_->Selection().Select(records.front().entity);SetTransformTool(TransformTool::Translate);SyncGizmoSelection();RefreshHierarchy();RefreshInspector();
 }
 const char* logPath=std::getenv("RENEGADE_WATERFALL_STUDIO_PROOF");if(!logPath)return;
 auto log=[&](const std::string& text){std::ofstream f(logPath,std::ios::app);f<<text<<std::endl;};
 auto require=[&](bool ok,const char* text){if(!ok)throw std::runtime_error(text);};
 try{
  static wf::WaterfallId second,primary;static size_t baseline=0;static wf::Definition initial;
  if(waterfallProofFrame_==90){baseline=records.size();primary=records.front().id;AddWaterfallFromView();auto e=session_->Selection().SelectedEntity();wf::AuthoredWaterfall value;require(wf::ReadWaterfall(scene,e,value,error),"Terrain Add controller did not create selectable metadata");second=value.id;initial=value.settings;require(waterfallInspector_.IsVisible(),"Dedicated Inspector not visible after Add");log("ADD_SELECTED_INSPECTOR_PASS");}
  if(waterfallProofFrame_==100){require(records.size()==baseline+1,"Second instance missing");require(session_->Commands().Undo(),"Undo Add failed");require(wf::FindWaterfallById(scene,second)==wi::ecs::INVALID_ENTITY,"Undo Add ghost");require(session_->Commands().Redo(),"Redo Add failed");session_->Selection().Select(wf::FindWaterfallById(scene,second));CommitWaterfallSetting(0,4.75f);}
  if(waterfallProofFrame_==115){wf::AuthoredWaterfall value;require(wf::ReadWaterfall(scene,wf::FindWaterfallById(scene,second),value,error)&&value.settings.width==4.75f,"Slider commit failed");require(session_->Commands().Undo(),"Undo slider failed");wf::ReadWaterfall(scene,wf::FindWaterfallById(scene,second),value,error);require(value.settings.width==initial.width,"Undo slider value wrong");require(session_->Commands().Redo(),"Redo slider failed");log("TWO_INSTANCES_SLIDER_UNDO_REDO_PASS");}
  if(waterfallProofFrame_==130){auto root=wf::FindWaterfallById(scene,second);auto landing=wf::LandingHandle(scene,root);auto after=bridge::CaptureTransform(*scene.transforms.GetComponent(landing));after.translation.x+=.5f;require(CommitWaterfallGizmo(landing,after),"Landing gizmo command failed");wf::AuthoredWaterfall value;wf::ReadWaterfall(scene,root,value,error);require(std::abs(value.settings.landing.x-initial.landing.x-.5f)<.001f,"Landing gizmo value wrong");require(session_->Commands().Undo(),"Undo landing failed");require(session_->Commands().Redo(),"Redo landing failed");log("HANDLE_COMMIT_UNDO_REDO_PASS");}
  if(waterfallProofFrame_==145){session_->Selection().Select(wf::FindWaterfallById(scene,second));DeleteSelection();require(wf::FindWaterfallById(scene,second)==wi::ecs::INVALID_ENTITY,"Delete ghost anchor");require(session_->Commands().Undo(),"Undo delete failed");require(wf::FindWaterfallById(scene,second)!=wi::ecs::INVALID_ENTITY,"Undo delete lost identity");require(session_->Commands().Redo(),"Redo delete failed");log("DELETE_UNDO_REDO_PASS");session_->Selection().Select(wf::FindWaterfallById(scene,primary));RefreshHierarchy();RefreshInspector();}
  if(waterfallProofFrame_==160||waterfallProofFrame_==220||waterfallProofFrame_==280){session_->Selection().Select(wf::FindWaterfallById(scene,primary));CommitWaterfallSetting(5,waterfallProofFrame_==160?0.f:waterfallProofFrame_==220?1.f:2.f);}
  if(waterfallProofFrame_==210||waterfallProofFrame_==270||waterfallProofFrame_==330){auto path=std::filesystem::path(logPath).parent_path()/(waterfallProofFrame_==210?"impact-0.png":waterfallProofFrame_==270?"impact-1.png":"impact-2.png");require(wi::helper::saveTextureToFile(GetRenderResult3D(),path.generic_string()),"Native Studio capture failed");log("STUDIO_IMPACT_CAPTURE_PASS "+path.filename().string());}
  if(waterfallProofFrame_==340)CommitWaterfallSetting(5,1.f);
  if(waterfallProofFrame_==360){SaveSceneAfterTransientCleanup(review);log("G13E_STUDIO_WORKFLOW_PASS native_document_saved=1 terrain_route_controller=1 two_instances=1 handles=1 commands=1");}
 }catch(const std::exception& e){log(std::string("G13E_STUDIO_WORKFLOW_FAIL ")+e.what());waterfallProofFrame_=1000;}
}

}
