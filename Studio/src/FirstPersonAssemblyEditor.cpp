#include "StudioApplication.h"
#include "renegade/bridge/FirstPersonAssemblyService.h"
#include "renegade/bridge/CreatorAssetWorkflowService.h"
#include <algorithm>
#include <filesystem>
namespace renegade::studio {
namespace {
const auto& Actions=bridge::FirstPersonAssemblyActions;
XMFLOAT3 Euler(const XMFLOAT4& q) {
 XMFLOAT4X4 m;XMStoreFloat4x4(&m,XMMatrixRotationQuaternion(XMLoadFloat4(&q)));
 const bool gimbal = std::abs(m._32) > 0.999999f;
 float p = gimbal ? std::copysign(XM_PIDIV2, -m._32) : std::asin(std::clamp(-m._32,-1.0f,1.0f));
 return {XMConvertToDegrees(p), gimbal ? 0.0f : XMConvertToDegrees(std::atan2(m._31,m._33)),
 XMConvertToDegrees(gimbal ? std::atan2(-m._21,m._11) : std::atan2(m._12,m._22))};
}
}
void StudioRenderPath::CreateAssemblyEditor() {
 assemblyPanel_.Create("First Person Assembly");
 assemblyPanel_.SetSize(XMFLOAT2(1080,810));assemblyPanel_.SetPos(XMFLOAT2(30,30));
 auto combo=[&](wi::gui::ComboBox& c,const std::string& name,float x,float y,float width){
 c.Create(name);c.SetPos(XMFLOAT2(x,y));c.SetSize(XMFLOAT2(width,26));assemblyPanel_.AddWidget(&c);};
 auto button=[&](wi::gui::Button& b,const char* name,float x,float y,float width){
 b.Create(name);b.SetText(name);b.SetPos(XMFLOAT2(x,y));b.SetSize(XMFLOAT2(width,28));assemblyPanel_.AddWidget(&b);};
 combo(assemblyArms_,"Arms product",110,35,420);
 combo(assemblyWeapon_,"Weapon product",110,70,420);
 auto partChanged=[this](const wi::gui::EventArgs&){
 if(assemblyRefreshing_)return;
 const auto before=assemblySettings_;
 int arms=assemblyArms_.GetSelected(),weapon=assemblyWeapon_.GetSelected();
 assemblySettings_={};
 if(arms>0&&size_t(arms)<assemblyPartIds_.size())assemblySettings_.armsAssetId=assemblyPartIds_[arms];
 if(weapon>0&&size_t(weapon)<assemblyPartIds_.size())assemblySettings_.weaponAssetId=assemblyPartIds_[weapon];
 RecordAssemblyDraft(before);
 assemblyPreviewRefreshPending_=false;assemblySave_.SetEnabled(false);
 assemblyStatus_.SetText("Parts changed. LOAD PARTS before preview or save.");
 };
 assemblyArms_.OnSelect(partChanged);assemblyWeapon_.OnSelect(partChanged);
 button(assemblyLoad_,"LOAD PARTS",20,105,510);
 assemblyLoad_.OnClick([this](const wi::gui::EventArgs&){
 wi::eventhandler::Subscribe_Once(wi::eventhandler::EVENT_THREAD_SAFE_POINT,[this](std::uint64_t){LoadAssemblyParts();});});
 assemblyImage_.Create("Assembly first-person preview");assemblyImage_.SetText("");
 assemblyImage_.SetColor(wi::Color(25,29,33,255));assemblyImage_.SetPos(XMFLOAT2(20,145));
 assemblyImage_.SetSize(XMFLOAT2(512,320));assemblyPanel_.AddWidget(&assemblyImage_);
 combo(assemblyBone_,"Parent bone",700,35,350);
 assemblyBone_.OnSelect([this](const wi::gui::EventArgs& a){
 if(assemblyRefreshing_)return;const auto before=assemblySettings_;size_t i=static_cast<size_t>(a.userdata);
 assemblySettings_.parentBonePath=i>0&&i<=assemblyBones_.size()?assemblyBones_[i-1].path:"";
 RecordAssemblyDraft(before);});
 const char* names[]={"Weapon X (m)","Weapon Y (m)","Weapon Z (m)","Weapon pitch","Weapon yaw","Weapon roll",
 "View X (m)","View Y (m)","View Z (m)","View pitch","View yaw","View roll"};
 for(int i=0;i<12;++i){
 auto& v=assemblyValues_[i];bool rotation=i%6>=3;
 v.Create(rotation?-180.0f:-10.0f,rotation?180.0f:10.0f,0,rotation?3600:20000,names[i],names[i]);
 v.SetPos(XMFLOAT2(700,80+i*31.0f));v.SetSize(XMFLOAT2(330,26));
 v.OnValueCommitted([this,i](float value){
 if(assemblyRefreshing_)return;const auto before=assemblySettings_;
 auto& p=i<6?assemblySettings_.weaponPosition:assemblySettings_.cameraPosition;
 auto& q=i<6?assemblySettings_.weaponRotation:assemblySettings_.cameraRotation;
 int a=i%6;
 if(a<3){if(a==0)p.x=value;else if(a==1)p.y=value;else p.z=value;}
 else {
 auto e=Euler(q);if(a==3)e.x=value;else if(a==4)e.y=value;else e.z=value;
 XMStoreFloat4(&q,XMQuaternionRotationRollPitchYaw(XMConvertToRadians(e.x),XMConvertToRadians(e.y),XMConvertToRadians(e.z)));
 }
 RecordAssemblyDraft(before);
 });assemblyPanel_.AddWidget(&v);
 }
 combo(assemblyActionPage_,"Action slots",550,105,130);assemblyActionPage_.SetText("");
 assemblyActionPage_.AddItem("Movement / use");
 assemblyActionPage_.AddItem("Aim / jump");
 assemblyActionPage_.AddItem("Land / partial");
 assemblyActionPage_.OnSelect([this](const wi::gui::EventArgs& a){
 for(size_t i=0;i<Actions.size();++i) {
 assemblyArmsClips_[i].SetVisible(i/6==size_t(std::max(a.iValue,0)));
 assemblyWeaponClips_[i].SetVisible(i/6==size_t(std::max(a.iValue,0)));
 }
 });
 // Explicit paired tracks. NONE leaves an action out of the assembled product.
 for(size_t i=0;i<Actions.size();++i){
 combo(assemblyArmsClips_[i],std::string(Actions[i])+" arms",140,490+(i%6)*31.0f,385);
 combo(assemblyWeaponClips_[i],std::string(Actions[i])+" weapon",665,490+(i%6)*31.0f,385);
 auto changed=[this](const wi::gui::EventArgs&){
 if(assemblyRefreshing_)return;
 const auto before=assemblySettings_;assemblySettings_.pairs.clear();
 for(size_t i=0;i<Actions.size();++i) {
 int a=assemblyArmsClips_[i].GetSelected(),w=assemblyWeaponClips_[i].GetSelected();
 if(a>0||w>0)assemblySettings_.pairs.push_back({Actions[i],a>0?unsigned(a-1):~0u,w>0?unsigned(w-1):~0u});
 }
 RecordAssemblyDraft(before);
 };
 assemblyArmsClips_[i].OnSelect(changed);assemblyWeaponClips_[i].OnSelect(changed);
 assemblyArmsClips_[i].SetVisible(i<6);assemblyWeaponClips_[i].SetVisible(i<6);
 }
 combo(assemblyAction_,"Preview action",680,460,190);
 assemblyAction_.OnSelect([this](const wi::gui::EventArgs& a){
 if(assemblyRefreshing_||!assemblyPreview_||a.iValue<0||size_t(a.iValue)>=assemblySettings_.pairs.size())return;
 assemblyPreview_->SetPairedAction(assemblySettings_.pairs[a.iValue].action);
 });
 button(assemblyPlay_,"PLAY",900,460,150);
 assemblyPlay_.OnClick([this](const wi::gui::EventArgs&){if(assemblyPreview_)assemblyPreview_->PlayPause();});
 assemblyTime_.Create(0,10,0,10000,"Paired time (seconds)");
 assemblyTime_.SetPos(XMFLOAT2(160,465));assemblyTime_.SetSize(XMFLOAT2(355,20));
 assemblyTime_.OnSlide([this](const wi::gui::EventArgs& a){if(assemblyPreview_)assemblyPreview_->Scrub(a.fValue);});
 assemblyPanel_.AddWidget(&assemblyTime_);
 assemblyName_.Create("Assembly name");assemblyName_.SetDescription("Copy name: ");
 assemblyName_.SetCancelInputEnabled(false);assemblyName_.SetPos(XMFLOAT2(160,682));assemblyName_.SetSize(XMFLOAT2(360,26));
 assemblyPanel_.AddWidget(&assemblyName_);
 button(assemblyPreviewButton_,"UPDATE PREVIEW",550,682,165);
 assemblyPreviewButton_.OnClick([this](const wi::gui::EventArgs&){
 wi::eventhandler::Subscribe_Once(wi::eventhandler::EVENT_THREAD_SAFE_POINT,[this](std::uint64_t){RebuildAssemblyPreview();});});
 button(assemblySave_,"SAVE CHANGES",725,682,165);
 assemblySave_.OnClick([this](const wi::gui::EventArgs&){
 wi::eventhandler::Subscribe_Once(wi::eventhandler::EVENT_THREAD_SAFE_POINT,[this](std::uint64_t){SaveAssemblyEditor(false);});});
 button(assemblySaveNew_,"SAVE AS NEW",725,752,165);
 assemblySaveNew_.OnClick([this](const wi::gui::EventArgs&){
 wi::eventhandler::Subscribe_Once(wi::eventhandler::EVENT_THREAD_SAFE_POINT,[this](std::uint64_t){SaveAssemblyEditor(true);});});
 button(assemblyUndo_,"UNDO",20,752,125);button(assemblyRedo_,"REDO",160,752,125);
 assemblyUndo_.OnClick([this](const wi::gui::EventArgs&){
 wi::eventhandler::Subscribe_Once(wi::eventhandler::EVENT_THREAD_SAFE_POINT,[this](std::uint64_t){
 if(assemblyCommands_.Undo())RefreshAssemblyDraft();});});
 assemblyRedo_.OnClick([this](const wi::gui::EventArgs&){
 wi::eventhandler::Subscribe_Once(wi::eventhandler::EVENT_THREAD_SAFE_POINT,[this](std::uint64_t){
 if(assemblyCommands_.Redo())RefreshAssemblyDraft();});});
 button(assemblyClose_,"CLOSE",900,682,150);
 assemblyClose_.OnClick([this](const wi::gui::EventArgs&){assemblyPanel_.SetVisible(false);assemblyPreview_.reset();assemblyImage_.SetImage({});});
 assemblyStatus_.Create("Assembly status");assemblyStatus_.SetPos(XMFLOAT2(20,720));
 assemblyStatus_.SetSize(XMFLOAT2(1030,25));assemblyStatus_.SetText("Select parts, explicit parent and clip pairs. Positions in metres; rotations in degrees.");
 assemblyPanel_.AddWidget(&assemblyStatus_);assemblyPanel_.SetVisible(false);GetGUI().AddWidget(&assemblyPanel_);
}
void StudioRenderPath::OpenAssemblyEditor() {
 if(!session_||!session_->Projects().HasProject()||!session_->Selection().HasSelection())return;
 auto entity=session_->Selection().SelectedEntity();auto& scene=session_->Scenes().GetScene();
 if(!bridge::IsPlayerStart(scene,entity))return;
 const auto project=session_->Projects().CurrentProject();
 const auto assigned=bridge::CapturePlayerControllerSettings(scene,entity).firstPersonArmsAssetId;
 wi::eventhandler::Subscribe_Once(wi::eventhandler::EVENT_THREAD_SAFE_POINT,[this,project,entity,assigned](std::uint64_t){
 if(!session_->Projects().HasProject()||session_->Projects().CurrentProject().projectId!=project.projectId)return;
 assemblyProjectId_=project.projectId;assemblyPlayer_=entity;assemblySettings_={};
 assemblyCommands_.Clear();assemblyAssetId_.clear();assemblyOriginalHash_.clear();assemblyRefreshing_=true;
 assemblyPreview_.reset();assemblyImage_.SetImage({});assemblyPartIds_.clear();
 assemblyArms_.ClearItems();assemblyWeapon_.ClearItems();
 assemblyArms_.AddItem("SELECT ARMS",0);assemblyWeapon_.AddItem("SELECT WEAPON",0);assemblyPartIds_.push_back({});
 bridge::AssetRegistry registry;std::string error;
 if(!bridge::CreatorAssetWorkflowService().RefreshRegistryFromDisk(project.rootPath,project.projectId,registry,error)){assemblyRefreshing_=false;studioChrome_.SetStatusText(error);return;}
 for(const auto& p:registry.importedProducts){
 const auto r=std::find_if(registry.records.begin(),registry.records.end(),[&](const auto& r){return r.assetId==p.productAssetId;});
 if(r==registry.records.end()||!r->sourceAvailable||std::filesystem::u8path(r->projectRelativePath).extension()!=".rasset"||
 p.importer=="renegade.first_person.assembly")continue;
 assemblyPartIds_.push_back(r->assetId);auto label=std::filesystem::u8path(r->projectRelativePath).filename().generic_u8string();
 assemblyArms_.AddItem(label,assemblyPartIds_.size()-1);assemblyWeapon_.AddItem(label,assemblyPartIds_.size()-1);
 }
 assemblyArms_.SetSelected(0);assemblyWeapon_.SetSelected(0);assemblyBone_.ClearItems();assemblyBones_.clear();
 for(size_t i=0;i<Actions.size();++i){assemblyArmsClips_[i].ClearItems();assemblyWeaponClips_[i].ClearItems();}
 assemblySave_.SetEnabled(false);assemblyName_.SetValue("First Person Assembly");assemblyPanel_.SetVisible(true);
 assemblyStatus_.SetText("Select parts and LOAD PARTS. Save as new creates and assigns a product.");
 assemblyRefreshing_=false;assemblyPreviewRefreshPending_=false;assemblyDraftPreviewDirty_=true;
 if(!assigned.empty()&&bridge::FirstPersonAssemblyService().ReadSettings(project.rootPath,project.projectId,assigned,assemblySettings_,error)){
 const auto record=std::find_if(registry.records.begin(),registry.records.end(),
 [&assigned](const auto& r){return r.assetId==assigned;});
 if(record!=registry.records.end()) {
 assemblyAssetId_=assigned;assemblyOriginalHash_=record->contentHash;
 assemblyName_.SetValue(std::filesystem::u8path(record->projectRelativePath).stem().generic_u8string());
 }
 assemblyRefreshing_=true;
 auto select=[&](wi::gui::ComboBox& box,const std::string& id){
 auto it=std::find(assemblyPartIds_.begin(),assemblyPartIds_.end(),id);
 if(it!=assemblyPartIds_.end())box.SetSelected(static_cast<int>(it-assemblyPartIds_.begin()));};
 select(assemblyArms_,assemblySettings_.armsAssetId);select(assemblyWeapon_,assemblySettings_.weaponAssetId);assemblyRefreshing_=false;LoadAssemblyParts();
 RebuildAssemblyPreview();
 }
 assemblyCommands_.Clear();assemblyCommands_.MarkSaved();
 });
}
void StudioRenderPath::LoadAssemblyParts() {
 if(!session_||!session_->Projects().HasProject()||session_->Projects().CurrentProject().projectId!=assemblyProjectId_)return;
 int a=assemblyArms_.GetSelected(),w=assemblyWeapon_.GetSelected();
 if(a<=0||w<=0||size_t(a)>=assemblyPartIds_.size()||size_t(w)>=assemblyPartIds_.size()){
 assemblyStatus_.SetText("Select both retained parts.");return;}
 auto& s=assemblySettings_;
 bool changed=s.armsAssetId!=assemblyPartIds_[a]||s.weaponAssetId!=assemblyPartIds_[w];
 if(changed){s={};s.armsAssetId=assemblyPartIds_[a];s.weaponAssetId=assemblyPartIds_[w];}
 const auto project=session_->Projects().CurrentProject();std::string error;
 auto arms=bridge::ReusableAssetService().PrepareModelAssetPlacement({project.rootPath,project.projectId,s.armsAssetId});
 auto weapon=bridge::ReusableAssetService().PrepareModelAssetPlacement({project.rootPath,project.projectId,s.weaponAssetId});
 if(!arms.IsReady()||!weapon.IsReady()){assemblyStatus_.SetText(!arms.IsReady()?arms.Result().error:weapon.Result().error);return;}
 if(!bridge::CollectPlayerViewBones(*arms.PeekScene(),assemblyBones_,error)){assemblyStatus_.SetText(error);return;}
 assemblyRefreshing_=true;assemblyBone_.ClearItems();assemblyBone_.AddItem("SELECT AUTHORED PARENT",0);
 int selected=0;for(size_t i=0;i<assemblyBones_.size();++i){
 assemblyBone_.AddItem(assemblyBones_[i].label,i+1);if(assemblyBones_[i].path==s.parentBonePath)selected=int(i+1);}
 assemblyBone_.SetSelected(selected);
 auto populate=[&](wi::gui::ComboBox& box,const wi::scene::Scene& scene,int chosen){
 box.ClearItems();box.AddItem("NONE",0);
 auto clips=bridge::CollectAnimationClips(scene, wi::ecs::INVALID_ENTITY, false);
 for(size_t c=0;c<clips.size();++c)box.AddItem(std::to_string(c)+" / "+clips[c].name,c+1);
 box.SetSelected(chosen);
 };
 for(size_t i=0;i<Actions.size();++i){
 auto p=std::find_if(s.pairs.begin(),s.pairs.end(),[&](const auto& p){return p.action==Actions[i];});
 populate(assemblyArmsClips_[i],*arms.PeekScene(),p==s.pairs.end()?0:int(p->armsClip+1));
 populate(assemblyWeaponClips_[i],*weapon.PeekScene(),p==s.pairs.end()?0:int(p->weaponClip+1));
 }
 auto we=Euler(s.weaponRotation),ce=Euler(s.cameraRotation);
 float values[]={s.weaponPosition.x,s.weaponPosition.y,s.weaponPosition.z,we.x,we.y,we.z,
 s.cameraPosition.x,s.cameraPosition.y,s.cameraPosition.z,ce.x,ce.y,ce.z};
 for(int i=0;i<12;++i)assemblyValues_[i].SetValue(values[i]);
 assemblyRefreshing_=false;assemblyPreviewRefreshPending_=false;assemblySave_.SetEnabled(false);
 QueueAssemblyPreviewRefresh();
}
void StudioRenderPath::RebuildAssemblyPreview() {
 assemblyPreviewRefreshPending_=false;assemblyDraftPreviewDirty_=true;
 if(!session_||!session_->Projects().HasProject()||session_->Projects().CurrentProject().projectId!=assemblyProjectId_)return;
 int armsIndex=assemblyArms_.GetSelected(),weaponIndex=assemblyWeapon_.GetSelected();
 if(armsIndex<=0||weaponIndex<=0||size_t(armsIndex)>=assemblyPartIds_.size()||size_t(weaponIndex)>=assemblyPartIds_.size()||
 assemblySettings_.armsAssetId!=assemblyPartIds_[armsIndex]||assemblySettings_.weaponAssetId!=assemblyPartIds_[weaponIndex]) {
 assemblyStatus_.SetText("LOAD PARTS for the selected products first.");return;
 }
 auto next=assemblySettings_;next.pairs.clear();
 for(size_t i=0;i<Actions.size();++i){
 int a=assemblyArmsClips_[i].GetSelected(),w=assemblyWeaponClips_[i].GetSelected();
 if((a>0)!=(w>0)){assemblyStatus_.SetText(std::string(Actions[i])+": choose both tracks or NONE for both.");return;}
 if(a>0)next.pairs.push_back({Actions[i],unsigned(a-1),unsigned(w-1)});
 }
 assemblySettings_=std::move(next);
 const auto project=session_->Projects().CurrentProject();wi::scene::Scene composed;std::string error;
 if(!bridge::FirstPersonAssemblyService().Prepare(project.rootPath,project.projectId,assemblySettings_,composed,error)){
 assemblyStatus_.SetText(error);return;}
 auto preview=std::make_unique<ModelImportPreview>();
 if(!preview->Prepare(composed,error)){assemblyStatus_.SetText(error);return;}
 const int selected=std::clamp(assemblyAction_.GetSelected(),0,int(assemblySettings_.pairs.size()-1));
 const float time=assemblyPreview_?assemblyPreview_->ClipTime():0;
 const bool playing=assemblyPreview_&&assemblyPreview_->IsPlaying();
 preview->UseFirstPersonCamera();preview->SetPairedAction(assemblySettings_.pairs[selected].action);
 preview->Scrub(time);if(playing)preview->PlayPause();
 assemblyRefreshing_=true;assemblyAction_.ClearItems();
 for(const auto& p:assemblySettings_.pairs)assemblyAction_.AddItem(p.action);
 assemblyAction_.SetSelected(selected);assemblyRefreshing_=false;
 float duration=0;for(const auto& clip:preview->Clips())duration=std::max(duration,clip.end-clip.start);
 bridge::AssetRegistry registry;
 if(!bridge::ReadAssetRegistry(project.rootPath,project.projectId,registry,error)){assemblyStatus_.SetText(error);return;}
 for(size_t i=0;i<2;++i){
 auto id=i==0?assemblySettings_.armsAssetId:assemblySettings_.weaponAssetId;
 auto record=std::find_if(registry.records.begin(),registry.records.end(),[&](const auto& r){return r.assetId==id;});
 if(record==registry.records.end()){assemblyStatus_.SetText("Part identity disappeared.");return;}
 assemblyPartHashes_[i]=record->contentHash;
 }
 assemblyTime_.SetRange(0,std::max(duration,0.001f));assemblyPreview_=std::move(preview);
 assemblyDraftPreviewDirty_=false;
 assemblyStatus_.SetText("Preview uses both native tracks on one clock. Shorter tracks hold their final pose.");
}
void StudioRenderPath::RecordAssemblyDraft(const bridge::FirstPersonAssemblySettings& before) {
 auto after=assemblySettings_;assemblySettings_=before;
 assemblyCommands_.Execute(std::make_unique<bridge::SetFirstPersonAssemblySettingsCommand>(assemblySettings_,std::move(after)));
 assemblySaveNew_.SetEnabled(false);
 QueueAssemblyPreviewRefresh();
}
void StudioRenderPath::QueueAssemblyPreviewRefresh() {
 assemblyDraftPreviewDirty_=true;assemblyPreviewRefreshPending_=true;
 assemblyPreviewRefreshDelay_=0.15f;
 assemblySave_.SetEnabled(false);assemblySaveNew_.SetEnabled(false);
 assemblyStatus_.SetText("Updating preview...");
}
void StudioRenderPath::RefreshAssemblyDraft() {
 assemblyRefreshing_=true;
 auto select=[&](wi::gui::ComboBox& box,const bridge::StableId& id){
 auto it=std::find(assemblyPartIds_.begin(),assemblyPartIds_.end(),id);
 box.SetSelected(it==assemblyPartIds_.end()?0:int(it-assemblyPartIds_.begin()));};
 select(assemblyArms_,assemblySettings_.armsAssetId);select(assemblyWeapon_,assemblySettings_.weaponAssetId);
 assemblyRefreshing_=false;LoadAssemblyParts();
 assemblySaveNew_.SetEnabled(false);
 QueueAssemblyPreviewRefresh();
}
void StudioRenderPath::SaveAssemblyEditor(bool asNew) {
 if(assemblyDraftPreviewDirty_||assemblyPreviewRefreshPending_||!assemblyPreview_||!assemblyPreview_->IsReady()||!session_->Projects().HasProject()||
 session_->Projects().CurrentProject().projectId!=assemblyProjectId_)return;
 const auto project=session_->Projects().CurrentProject();std::string error;bridge::StableId id;std::vector<std::uint8_t> thumbnail;
 bridge::AssetRegistry registry;
 if(!bridge::ReadAssetRegistry(project.rootPath,project.projectId,registry,error)){assemblyStatus_.SetText(error);return;}
 for(size_t i=0;i<2;++i){
 auto id=i==0?assemblySettings_.armsAssetId:assemblySettings_.weaponAssetId;
 auto record=std::find_if(registry.records.begin(),registry.records.end(),[&](const auto& r){return r.assetId==id;});
 if(record==registry.records.end()||record->contentHash!=assemblyPartHashes_[i]){
 assemblyStatus_.SetText("A part changed after preview. LOAD PARTS and UPDATE PREVIEW before saving.");assemblyPreview_.reset();return;}
 }
 if(!assemblyPreview_->CapturePng(thumbnail,error)){assemblyStatus_.SetText(error);return;}
 if(!asNew) {
 if(assemblyAssetId_.empty()){assemblyStatus_.SetText("Use SAVE AS NEW for the first save.");return;}
 id=assemblyAssetId_;
 if(!bridge::FirstPersonAssemblyService().Update(project.rootPath,project.projectId,id,assemblyOriginalHash_,
 assemblySettings_,thumbnail,error)){assemblyStatus_.SetText(error);return;}
 } else if(!bridge::FirstPersonAssemblyService().Save(project.rootPath,project.projectId,assemblyName_.GetValue(),
 assemblySettings_,thumbnail,id,error)){assemblyStatus_.SetText(error);return;}
 assemblyAssetId_=id;assemblyCommands_.MarkSaved();
 bridge::AssetRegistry updated;
 if(bridge::ReadAssetRegistry(project.rootPath,project.projectId,updated,error)) {
 auto record=std::find_if(updated.records.begin(),updated.records.end(),[&](const auto& r){return r.assetId==id;});
 if(record!=updated.records.end())assemblyOriginalHash_=record->contentHash;
 }
 auto& scene=session_->Scenes().GetScene();
 bool assigned=false;
 if(asNew&&bridge::IsPlayerStart(scene,assemblyPlayer_)){
 auto settings=bridge::CapturePlayerControllerSettings(scene,assemblyPlayer_);settings.firstPersonArmsAssetId=id;
 assigned=session_->Commands().Execute(std::make_unique<bridge::SetPlayerControllerSettingsCommand>(scene,assemblyPlayer_,settings));
 }
 assemblyStatus_.SetText(!asNew ? "Saved changes to the existing asset. Player Start assignment preserved." : assigned ? "Saved and assigned. Save the level to retain Player Start assignment." :
 "Assembly saved, but Player Start assignment failed. Select the saved asset on Player Start.");
 assemblySave_.SetEnabled(false);RefreshAssetBrowser();RefreshInspector();RefreshStatus();
}
}
