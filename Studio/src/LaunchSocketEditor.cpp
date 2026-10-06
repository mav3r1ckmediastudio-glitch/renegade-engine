#include "StudioApplication.h"
#include "renegade/bridge/LaunchSocketService.h"
#include <algorithm>
#include <cmath>
namespace renegade::studio {
void StudioRenderPath::CreateLaunchSocketEditor() {
 launchSocketPanel_.Create("Weapon launch sockets");
 launchSocketPanel_.SetPos({120,100});launchSocketPanel_.SetSize({1260,650});
 const auto combo=[&](wi::gui::ComboBox& c,const char* name,float y) {
  c.Create(name);c.SetPos({220,y});c.SetSize({390,26});launchSocketPanel_.AddWidget(&c);
 };
 const auto button=[&](wi::gui::Button& b,const char* name,float x,float y,float width) {
  b.Create(name);b.SetText(name);b.SetPos({x,y});b.SetSize({width,30});launchSocketPanel_.AddWidget(&b);
 };
 combo(launchSocketChoice_,"Socket",40);combo(launchSocketPart_,"Attached model",120);
 launchSocketPart_.AddItem("Primary weapon");launchSocketPart_.AddItem("Off-hand weapon");launchSocketPart_.AddItem("Arms / palm");
 combo(launchSocketParent_,"Follow bone / marker",160);
 launchSocketName_.Create("Socket name");launchSocketName_.SetDescription("Name: ");
 launchSocketName_.SetPos({220,80});launchSocketName_.SetSize({390,26});
 launchSocketName_.SetCancelInputEnabled(false);launchSocketPanel_.AddWidget(&launchSocketName_);
 launchSocketName_.OnInputAccepted([this](const wi::gui::EventArgs&){
  if(launchSocketRefreshing_||launchSocketDraft_.empty())return;
  launchSocketDraft_[launchSocketSelected_].name=launchSocketName_.GetText();
  RefreshLaunchSocketEditor();
 });
 launchSocketChoice_.OnSelect([this](const wi::gui::EventArgs& a){
  if(launchSocketRefreshing_||a.iValue<0||size_t(a.iValue)>=launchSocketDraft_.size())return;
  launchSocketSelected_=a.iValue;launchSocketReload_=true;RefreshLaunchSocketEditor();
 });
 launchSocketPart_.OnSelect([this](const wi::gui::EventArgs& a){
  if(launchSocketRefreshing_||launchSocketDraft_.empty())return;
  auto& s=launchSocketDraft_[launchSocketSelected_];s.part=bridge::LaunchSocketPart(a.iValue);
  s.parentPath.clear();s.position={};s.rotationDegrees={};
  launchSocketReload_=true;RefreshLaunchSocketEditor();
 });
 launchSocketParent_.OnSelect([this](const wi::gui::EventArgs& a){
  if(launchSocketRefreshing_||launchSocketDraft_.empty())return;
  auto& s=launchSocketDraft_[launchSocketSelected_];
  s.parentPath=a.iValue<=0?std::string{}:launchSocketParents_[a.iValue-1].path;
  s.position={};s.rotationDegrees={};launchSocketChanged_=true;RefreshLaunchSocketEditor();
 });
 const char* labels[]={"Position X (m)","Position Y (m)","Position Z (m)",
  "Direction pitch","Direction yaw","Direction roll"};
 for(unsigned i=0;i<6;++i) {
  auto& value=launchSocketValues_[i];value.Create(i<3?-10.0f:-180.0f,i<3?10.0f:180.0f,0,10000,labels[i],labels[i]);
  value.SetPos({220,float(i<3?210+i*40:350+(i-3)*40)});value.SetSize({390,26});
  value.SetTooltip(i<3?"Socket offset in metres, relative to the selected model or bone.":"Direction in degrees. The orange arrow shows where the projectile leaves.");
  launchSocketPanel_.AddWidget(&value);
  value.OnValueCommitted([this,i](float v){
   if(launchSocketRefreshing_||launchSocketDraft_.empty())return;
   auto& s=launchSocketDraft_[launchSocketSelected_];
   auto& vector=i<3?s.position:s.rotationDegrees;(&vector.x)[i%3]=v;
   launchSocketChanged_=true;
  });
 }
 button(launchSocketNew_,"NEW",625,40,80);button(launchSocketRemove_,"REMOVE",625,80,80);
 launchSocketNew_.OnClick([this](const wi::gui::EventArgs&){
  if(launchSocketDraft_.size()>=16)return;
  bridge::LaunchSocketDefinition s;
  unsigned count=1;
  while(std::any_of(launchSocketDraft_.begin(),launchSocketDraft_.end(),[&](const auto& p){return p.name==s.name;}))
   s.name="Muzzle "+std::to_string(++count);
  launchSocketDraft_.push_back(s);launchSocketSelected_=int(launchSocketDraft_.size()-1);
  launchSocketReload_=true;RefreshLaunchSocketEditor();
 });
 launchSocketRemove_.OnClick([this](const wi::gui::EventArgs&){
  if(launchSocketDraft_.empty())return;
  launchSocketDraft_.erase(launchSocketDraft_.begin()+launchSocketSelected_);
  launchSocketSelected_=std::max(0,std::min(launchSocketSelected_,int(launchSocketDraft_.size())-1));
  launchSocketReload_=true;RefreshLaunchSocketEditor();
 });
 button(launchSocketPlace_,"PLACE ON MODEL",220,485,185);
 launchSocketPlace_.OnClick([this](const wi::gui::EventArgs&){launchSocketPlacing_=!launchSocketPlacing_;launchSocketDrag_=0;});
 button(launchSocketFireSpot_,"USE FIRESPOT",420,485,190);
 launchSocketFireSpot_.OnClick([this](const wi::gui::EventArgs&){
  if(launchSocketDraft_.empty())return;
  int match=-1;
  for(size_t i=0;i<launchSocketParents_.size();++i) {
   const auto& label=launchSocketParents_[i].label;
   if(label=="FIRESPOT" || (label.size()>=11&&label.substr(label.size()-11)==" > FIRESPOT")) {
    if(match>=0){launchSocketHelp_.SetText("More than one FIRESPOT exists. Choose the exact marker from the list.");return;}
    match=int(i);
   }
  }
  if(match<0){launchSocketHelp_.SetText("This model has no unambiguous FIRESPOT. Use PLACE ON MODEL or choose another bone.");return;}
  auto& s=launchSocketDraft_[launchSocketSelected_];s.parentPath=launchSocketParents_[match].path;
  s.position={};s.rotationDegrees={};launchSocketChanged_=true;RefreshLaunchSocketEditor();
 });
 button(launchSocketApply_,"APPLY TO ASSEMBLY",220,580,225);
 button(launchSocketClose_,"CANCEL",460,580,150);
 launchSocketClose_.OnClick([this](const wi::gui::EventArgs&){
  launchSocketPanel_.SetVisible(false);assemblyPanel_.SetVisible(true);assemblyPanel_.SetEnabled(true);
  launchSocketRestoreAssembly_=false;
  launchSocketPreview_.reset();launchSocketImage_.SetImage({});launchSocketDrag_=0;
 });
 launchSocketApply_.OnClick([this](const wi::gui::EventArgs&){
  wi::eventhandler::Subscribe_Once(wi::eventhandler::EVENT_THREAD_SAFE_POINT,[this](std::uint64_t){
   std::string error;
   auto next=assemblySettings_;next.launchSockets=launchSocketDraft_;
   const auto& project=session_->Projects().CurrentProject();wi::scene::Scene prepared;
   if(!bridge::FirstPersonAssemblyService().Prepare(project.rootPath,project.projectId,next,prepared,error)) {
    launchSocketHelp_.SetText(error);return;
   }
   const auto before=assemblySettings_;assemblySettings_=std::move(next);RecordAssemblyDraft(before);
   launchSocketPanel_.SetVisible(false);assemblyPanel_.SetVisible(true);assemblyPanel_.SetEnabled(true);
  launchSocketRestoreAssembly_=false;
   launchSocketPreview_.reset();launchSocketImage_.SetImage({});launchSocketDrag_=0;
   assemblyStatus_.SetText("Sockets applied to the draft. SAVE CHANGES to retain them; then select one in Weapon Projectiles.");
  });
 });
 launchSocketImage_.Create("Socket model preview");launchSocketImage_.SetText("");
 launchSocketImage_.SetPos({720,80});launchSocketImage_.SetSize({512,320});
 launchSocketImage_.SetColor(wi::Color(22,26,33));launchSocketPanel_.AddWidget(&launchSocketImage_);
 button(launchSocketFit_,"FIT / RESET VIEW",720,420,180);
 launchSocketFit_.OnClick([this](const wi::gui::EventArgs&){
  if(launchSocketPreview_){launchSocketPreview_->FitModel();launchSocketPreview_->SetView(.5f,.2425f);}
 });
 launchSocketHelp_.Create("Socket help");launchSocketHelp_.SetPos({720,475});launchSocketHelp_.SetSize({512,140});
 launchSocketHelp_.font.params.size=14;launchSocketPanel_.AddWidget(&launchSocketHelp_);
 launchSocketPanel_.SetVisible(false);GetGUI().AddWidget(&launchSocketPanel_);
}
void StudioRenderPath::OpenLaunchSocketEditor() {
 launchSocketDraft_=assemblySettings_.launchSockets;
 if(launchSocketDraft_.empty())launchSocketDraft_.push_back({});
 launchSocketSelected_=0;launchSocketReload_=true;launchSocketPlacing_=false;launchSocketDrag_=0;
 launchSocketRestoreAssembly_=true;
 assemblyPanel_.SetVisible(false);assemblyPanel_.SetEnabled(false);
 launchSocketPanel_.SetVisible(true);launchSocketPanel_.Activate();
 RefreshLaunchSocketEditor();
}
void StudioRenderPath::RefreshLaunchSocketEditor() {
 launchSocketRefreshing_=true;
 launchSocketChoice_.ClearItems();
 for(const auto& s:launchSocketDraft_)launchSocketChoice_.AddItem(s.name);
 if(!launchSocketDraft_.empty()) {
  const auto& s=launchSocketDraft_[launchSocketSelected_];
  launchSocketChoice_.SetSelectedWithoutCallback(launchSocketSelected_);
  launchSocketName_.SetText(s.name);launchSocketPart_.SetSelectedWithoutCallback(int(s.part));
  float values[]={s.position.x,s.position.y,s.position.z,s.rotationDegrees.x,s.rotationDegrees.y,s.rotationDegrees.z};
  for(unsigned i=0;i<6;++i)launchSocketValues_[i].SetValue(values[i]);
  launchSocketParent_.ClearItems();launchSocketParent_.AddItem("Model root (no separate bone)");
  int selected=0;
  const auto shortLabel=[](const auto& parent) {
   const auto split=parent.label.rfind(" > ");
   return split==std::string::npos?parent.label:parent.label.substr(split+3);
  };
  for(size_t i=0;i<launchSocketParents_.size();++i) {
   const auto leaf=shortLabel(launchSocketParents_[i]);
   const auto duplicates=std::count_if(launchSocketParents_.begin(),launchSocketParents_.end(),
    [&](const auto& p){return shortLabel(p)==leaf;});
   launchSocketParent_.AddItem(duplicates>1?launchSocketParents_[i].label:leaf);
   if(launchSocketParents_[i].path==s.parentPath)selected=int(i+1);
  }
  launchSocketParent_.SetSelectedWithoutCallback(selected);
  launchSocketParent_.SetTooltip(selected?launchSocketParents_[selected-1].label:
   "Follows the whole model. Choose a bone when the socket must follow an animated part.");
 }
 launchSocketRefreshing_=false;launchSocketChanged_=true;
}
void StudioRenderPath::UpdateLaunchSocketEditor(float dt) {
 if(!launchSocketPanel_.IsVisible()||!session_||
    !session_->Projects().HasProject()||session_->Projects().CurrentProject().projectId!=assemblyProjectId_) {
  if(launchSocketRestoreAssembly_) {
   if(session_ && session_->Projects().HasProject() &&
      session_->Projects().CurrentProject().projectId==assemblyProjectId_)assemblyPanel_.SetVisible(true);
   assemblyPanel_.SetEnabled(true);launchSocketRestoreAssembly_=false;
  }
  launchSocketPanel_.SetVisible(false);launchSocketPreview_.reset();launchSocketImage_.SetImage({});
  launchSocketDrag_=0;return;
 }
 const bool hasSocket=!launchSocketDraft_.empty();
 launchSocketPlace_.SetEnabled(hasSocket);launchSocketFireSpot_.SetEnabled(hasSocket);
 launchSocketName_.SetEnabled(hasSocket);launchSocketPart_.SetEnabled(hasSocket);launchSocketParent_.SetEnabled(hasSocket);
 for(auto& value:launchSocketValues_)value.SetEnabled(hasSocket);
 if(!hasSocket) {
  launchSocketPreview_.reset();launchSocketImage_.SetImage({});launchSocketDrag_=0;
  launchSocketHelp_.SetText("No sockets. NEW creates one; APPLY removes all saved sockets from this assembly draft.");
  launchSocketApply_.SetEnabled(true);return;
 }
 const auto& project=session_->Projects().CurrentProject();
 auto& socket=launchSocketDraft_[launchSocketSelected_];
 if(launchSocketReload_) {
  launchSocketReload_=false;launchSocketPreview_.reset();launchSocketParents_.clear();
  launchSocketImage_.SetColor(wi::Color(22,26,33));launchSocketImage_.SetImage({});
  launchSocketDrag_=0;launchSocketPlacing_=false;
  const auto asset=socket.part==bridge::LaunchSocketPart::PrimaryWeapon?assemblySettings_.weaponAssetId:
   socket.part==bridge::LaunchSocketPart::OffHandWeapon?assemblySettings_.offHandWeaponAssetId:assemblySettings_.armsAssetId;
  auto prepared=bridge::ReusableAssetService().PrepareModelAssetPlacement({project.rootPath,project.projectId,asset});
  std::string error;
  if(!prepared.IsReady())error=prepared.Result().error;
  else {
   auto native=prepared.ReleaseScene();auto preview=std::make_unique<ModelImportPreview>();
   if(preview->Prepare(*native,error)) {
    launchSocketParents_=preview->SocketParents();launchSocketPreview_=std::move(preview);
   }
  }
  if(!launchSocketPreview_){launchSocketHelp_.SetText(error);launchSocketApply_.SetEnabled(false);return;}
  RefreshLaunchSocketEditor();
 }
 if(!launchSocketPreview_){launchSocketApply_.SetEnabled(false);return;}
 std::string error;
 const bool valid=bridge::ValidateLaunchSockets(launchSocketDraft_,error);
 if(!valid){launchSocketHelp_.SetText(error);launchSocketApply_.SetEnabled(false);launchSocketDrag_=0;return;}
 if(launchSocketChanged_) {
  launchSocketChanged_=false;
  if(!launchSocketPreview_->ShowSocket(socket,error)) {
   launchSocketChanged_=true;launchSocketHelp_.SetText(error);launchSocketApply_.SetEnabled(false);return;
  }
  launchSocketHelp_.SetText("Orange arrow = launch direction.\nLeft-drag: orbit | Right-drag: pan | Wheel: zoom\nPLACE ON MODEL: click a surface to position the socket.\nChoose a bone to follow animation, or use FIRESPOT.\nApply, then SAVE CHANGES in the assembly editor.");
 }
 const auto pointer=wi::input::GetPointer();const auto pos=launchSocketImage_.GetPos();const auto size=launchSocketImage_.GetSize();
 const bool inside=pointer.x>=pos.x&&pointer.x<pos.x+size.x&&pointer.y>=pos.y&&pointer.y<pos.y+size.y;
 const auto dragButton=launchSocketDrag_==2?wi::input::MOUSE_BUTTON_RIGHT:wi::input::MOUSE_BUTTON_LEFT;
 if(launchSocketDrag_&&!wi::input::Down(dragButton))launchSocketDrag_=0;
 if(!launchSocketDrag_&&inside) {
  if(wi::input::Press(wi::input::MOUSE_BUTTON_LEFT)) {
   if(launchSocketPlacing_) {
    XMFLOAT3 point;
    if(launchSocketPreview_->PickSocket((pointer.x-pos.x)/size.x,(pointer.y-pos.y)/size.y,socket,point,error)) {
     socket.position=point;launchSocketPlacing_=false;RefreshLaunchSocketEditor();
    } else launchSocketHelp_.SetText(error);
   } else launchSocketDrag_=1;
  } else if(wi::input::Press(wi::input::MOUSE_BUTTON_RIGHT))launchSocketDrag_=2;
  if(launchSocketDrag_)launchSocketPointer_={pointer.x,pointer.y};
 }
 if(launchSocketDrag_) {
  const float dx=pointer.x-launchSocketPointer_.x,dy=pointer.y-launchSocketPointer_.y;
  if(dx!=0||dy!=0) {
   if(launchSocketDrag_==1)launchSocketPreview_->Orbit(-dx*.008f,dy*.008f);
   else launchSocketPreview_->Pan(dx/std::max(1.0f,size.y),dy/std::max(1.0f,size.y));
  }
  launchSocketPointer_={pointer.x,pointer.y};
 }
 if(inside&&pointer.z!=0)launchSocketPreview_->Zoom(std::pow(.85f,std::clamp(pointer.z,-8.0f,8.0f)));
 launchSocketPlace_.SetText(launchSocketPlacing_?"CLICK MODEL / CANCEL":"PLACE ON MODEL");
 if(launchSocketPreview_->NeedsRender()){launchSocketPreview_->PreUpdate();launchSocketPreview_->Update(dt);}
 if(launchSocketPreview_->IsReady()) {
  wi::Resource image;image.SetTexture(launchSocketPreview_->GetRenderResult3D());
  launchSocketImage_.SetColor(wi::Color::White());
  for(auto& sprite:launchSocketImage_.sprites)sprite.params.disableBackground();
  launchSocketImage_.SetImage(image);
 }
 launchSocketApply_.SetEnabled(launchSocketPreview_->IsReady());
}
}
