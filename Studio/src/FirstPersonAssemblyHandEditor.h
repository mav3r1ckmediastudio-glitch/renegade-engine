// Included by FirstPersonAssemblyEditor.cpp within renegade::studio.
void StudioRenderPath::ShowAssemblyHandPage(int page) {
 assemblyOffWeapon_.SetVisible(page==0);assemblyPreviewShield_.SetVisible(page==0);
 for(auto& c:assemblyHandBones_)c.SetVisible(page==0);
 for(auto& c:assemblyDirectionalClips_)c.SetVisible(page==1);
 for(auto& c:assemblyBlockClips_)c.SetVisible(page==2);
 for(auto& c:assemblyAttackVariants_)c.SetVisible(page==2);
 for(size_t i=0;i<assemblyHandValues_.size();++i)
  assemblyHandValues_[i].SetVisible(i<8?page==0:i<22?page==3:page==4);
 assemblyAvoidance_.SetVisible(page==3);
 const char* help[]={
  "Primary weapon and grip are in the main window. Choose an off-hand mesh for independent arms.\nBone roots partition animation ownership; anchors attach meshes. Static meshes need no weapon clips.\nChanging only the sword preserves all animation bindings. Use SAVE AS NEW to keep the original.",
  "Map all four Charge / Hold / Release groups, or leave all twelve NONE.\nEach source clip must have one role. Directional control uses mouse movement while holding LMB.",
  "Shield Raise / Hold / Lower control the off-hand independently.\nAttack variants support non-directional sets and provide the required fallback attack.\nEach source clip must have one role; NONE removes a variant.",
  "Blade endpoints and radius are in the sword attachment's local space.\nShield center and half-size are in the shield attachment's local space, before mesh scale.\nCorrection is bounded arm movement. It is presentation avoidance, not world or damage collision.",
  "Full charge sets the time to reach 100%. Chain window accepts input near the end of a strike.\nReleased queue lifetime starts on LMB release and waits for authored equipment recovery.\nEquipment action preparation / windup / recovery remain in EQUIPMENT settings."
 };
 assemblyHandHelp_.SetText(help[std::clamp(page,0,4)]);
}
void StudioRenderPath::CreateAssemblyHandEditor() {
 assemblyHandPanel_.Create("Hand and melee setup");
 assemblyHandPanel_.SetPos({1120,30});assemblyHandPanel_.SetSize({730,780});
 auto combo=[&](wi::gui::ComboBox& c,const std::string& name,float y) {
  c.Create(name);c.SetPos({225,y});c.SetSize({475,26});assemblyHandPanel_.AddWidget(&c);
 };
 combo(assemblyHandPage_,"Section",35);
 for(const char* name:{"Attachments","Directional strikes","Block / attack variants","Collision proxies","Melee timing"})assemblyHandPage_.AddItem(name);
 assemblyHandPage_.OnSelect([this](const wi::gui::EventArgs& a){ShowAssemblyHandPage(a.iValue);});
 combo(assemblyOffWeapon_,"Off-hand mesh",80);
 assemblyOffWeapon_.OnSelect([this](const wi::gui::EventArgs& a){
  if(assemblyRefreshing_)return;
  const auto before=assemblySettings_;
  assemblySettings_.offHandWeaponAssetId=a.userdata>0&&size_t(a.userdata)<assemblyPartIds_.size()?assemblyPartIds_[a.userdata]:"";
  RecordAssemblyDraft(before);LoadAssemblyParts();
 });
 const char* boneLabels[]={"Off-hand anchor","Primary layer root","Off-hand layer root"};
 for(int i=0;i<3;++i) {
  combo(assemblyHandBones_[i],boneLabels[i],115+i*35);
  assemblyHandBones_[i].OnSelect([this,i](const wi::gui::EventArgs& a) {
   if(assemblyRefreshing_)return;const auto before=assemblySettings_;
   std::string* paths[]={&assemblySettings_.offHandParentBonePath,&assemblySettings_.primaryLayerRootPath,&assemblySettings_.offHandLayerRootPath};
   *paths[i]=a.userdata>0&&size_t(a.userdata)<=assemblyBones_.size()?assemblyBones_[a.userdata-1].path:"";
   RecordAssemblyDraft(before);
  });
 }
 const char* directions[]={"Left","Right","Down","Stab"};const char* stages[]={"Charge","Hold","Release"};
 for(int i=0;i<12;++i) {
  combo(assemblyDirectionalClips_[i],std::string(directions[i/3])+" "+stages[i%3],80+i*36);
  assemblyDirectionalClips_[i].OnSelect([this,i](const wi::gui::EventArgs& a) {
   if(assemblyRefreshing_)return;const auto before=assemblySettings_;
   auto& pairs=assemblySettings_.pairs;const std::string action=bridge::FirstPersonDirectionalActions[i];
   pairs.erase(std::remove_if(pairs.begin(),pairs.end(),[&](const auto& p){return p.action==action;}),pairs.end());
   if(a.iValue>0)pairs.push_back({action,unsigned(a.iValue-1),0});
   RecordAssemblyDraft(before);
  });
 }
 const char* blocks[]={"Shield raise","Shield hold","Shield lower"};
 for(int i=0;i<3;++i) {
  combo(assemblyBlockClips_[i],blocks[i],80+i*40);
  assemblyBlockClips_[i].OnSelect([this,i](const wi::gui::EventArgs& a) {
   if(assemblyRefreshing_)return;const auto before=assemblySettings_;
   unsigned* clips[]={&assemblySettings_.blockStartClip,&assemblySettings_.blockLoopClip,&assemblySettings_.blockEndClip};
   *clips[i]=a.iValue>0?unsigned(a.iValue-1):~0u;RecordAssemblyDraft(before);
  });
 }
 for(int i=0;i<4;++i) {
  combo(assemblyAttackVariants_[i],"Attack variant "+std::to_string(i+1),230+i*40);
  assemblyAttackVariants_[i].OnSelect([this](const wi::gui::EventArgs&) {
   if(assemblyRefreshing_)return;const auto before=assemblySettings_;auto& p=assemblySettings_.pairs;
   p.erase(std::remove_if(p.begin(),p.end(),[](const auto& p){return p.action=="Attack";}),p.end());
   for(auto& c:assemblyAttackVariants_)if(c.GetSelected()>0)p.push_back({"Attack",unsigned(c.GetSelected()-1),0});
   RecordAssemblyDraft(before);
  });
 }
 const char* labels[]={"Shield X (m)","Shield Y (m)","Shield Z (m)","Shield pitch","Shield yaw","Shield roll",
  "Sword scale","Shield scale","Blade base X","Blade base Y","Blade base Z","Blade tip X","Blade tip Y","Blade tip Z",
  "Shield center X","Shield center Y","Shield center Z","Shield half X","Shield half Y","Shield half Z",
  "Blade radius (m)","Max correction (m)","Full charge (s)","Chain window (s)","Queue lifetime (s)"};
 for(int i=0;i<25;++i) {
  auto& v=assemblyHandValues_[i];
  float low=-10,high=10;
  if(i>=3&&i<6){low=-180;high=180;}
  if(i==6||i==7){low=0.01f;high=100;}
  if(i>=17&&i<=20){low=0.001f;high=i==20?0.1f:10;}
  if(i==21){low=0.001f;high=0.5f;}
  if(i==22){low=0.1f;high=10;}if(i==23){low=0;high=2;}if(i==24){low=0.05f;high=5;}
  v.Create(low,high,0,20000,labels[i],labels[i]);
  float y=i<8?225+i*35:i<22?120+(i-8)*34:100+(i-22)*45;
  v.SetPos({225,y});v.SetSize({475,26});assemblyHandPanel_.AddWidget(&v);
  v.OnValueCommitted([this,i](float value) {
   if(assemblyRefreshing_)return;const auto before=assemblySettings_;auto& s=assemblySettings_;
   if(i<3){float* a[]={&s.offHandWeaponPosition.x,&s.offHandWeaponPosition.y,&s.offHandWeaponPosition.z};*a[i]=value;}
   else if(i<6){auto e=Euler(s.offHandWeaponRotation);if(i==3)e.x=value;if(i==4)e.y=value;if(i==5)e.z=value;
    XMStoreFloat4(&s.offHandWeaponRotation,XMQuaternionRotationRollPitchYaw(XMConvertToRadians(e.x),XMConvertToRadians(e.y),XMConvertToRadians(e.z)));}
   else {
    float* a[]={&s.weaponScale,&s.offHandWeaponScale,&s.bladeBase.x,&s.bladeBase.y,&s.bladeBase.z,
     &s.bladeTip.x,&s.bladeTip.y,&s.bladeTip.z,&s.shieldCenter.x,&s.shieldCenter.y,&s.shieldCenter.z,
     &s.shieldHalfExtents.x,&s.shieldHalfExtents.y,&s.shieldHalfExtents.z,&s.bladeRadius,&s.maximumHandCorrection,
     &s.fullChargeSeconds,&s.chainWindowSeconds,&s.queuedReleaseSeconds};
    *a[i-6]=value;
   }
   RecordAssemblyDraft(before);
  });
 }
 assemblyPreviewShield_.Create("Preview with shield held");assemblyPreviewShield_.SetPos({225,555});assemblyPreviewShield_.SetSize({22,22});
 assemblyPreviewShield_.OnClick([this](const wi::gui::EventArgs&){if(!assemblyRefreshing_)QueueAssemblyPreviewRefresh();});
 assemblyHandPanel_.AddWidget(&assemblyPreviewShield_);
 assemblyAvoidance_.Create("Avoid sword / shield clipping");assemblyAvoidance_.SetPos({225,80});assemblyAvoidance_.SetSize({22,22});
 assemblyAvoidance_.OnClick([this](const wi::gui::EventArgs& a){if(assemblyRefreshing_)return;const auto before=assemblySettings_;assemblySettings_.avoidOffHand=a.bValue;RecordAssemblyDraft(before);});
 assemblyHandPanel_.AddWidget(&assemblyAvoidance_);
 assemblyHandHelp_.Create("Hand setup help");assemblyHandHelp_.SetPos({20,640});assemblyHandHelp_.SetSize({690,100});
 assemblyHandHelp_.font.params.size=14;assemblyHandPanel_.AddWidget(&assemblyHandHelp_);
 assemblyHandPage_.SetSelected(0);ShowAssemblyHandPage(0);assemblyHandPanel_.SetVisible(false);GetGUI().AddWidget(&assemblyHandPanel_);
}
void StudioRenderPath::RefreshAssemblyHandEditor(const wi::scene::Scene& arms) {
 const auto& s=assemblySettings_;
 assemblyOffWeapon_.SetSelected(0);
 for(size_t row=1;row<assemblyOffWeapon_.GetItemCount();++row) {
  auto index=assemblyOffWeapon_.GetItemUserData(int(row));
  if(index<assemblyPartIds_.size()&&assemblyPartIds_[index]==s.offHandWeaponAssetId)assemblyOffWeapon_.SetSelected(int(row));
 }
 const std::string paths[]={s.offHandParentBonePath,s.primaryLayerRootPath,s.offHandLayerRootPath};
 for(int i=0;i<3;++i) {
  auto& c=assemblyHandBones_[i];c.ClearItems();c.AddItem("SELECT BONE",0);int selected=0;
  for(size_t b=0;b<assemblyBones_.size();++b){c.AddItem(assemblyBones_[b].label,b+1);if(assemblyBones_[b].path==paths[i])selected=int(b+1);}
  c.SetSelected(selected);
 }
 auto clips=bridge::CollectAnimationClips(arms,wi::ecs::INVALID_ENTITY,false);
 auto populate=[&](wi::gui::ComboBox& c,unsigned selected) {
  c.ClearItems();c.AddItem("NONE",0);for(size_t i=0;i<clips.size();++i)c.AddItem(std::to_string(i)+" / "+clips[i].name,i+1);
  c.SetSelected(selected<clips.size()?int(selected+1):0);
 };
 for(int i=0;i<12;++i) {
  auto p=std::find_if(s.pairs.begin(),s.pairs.end(),[&](const auto& p){return p.action==bridge::FirstPersonDirectionalActions[i];});
  populate(assemblyDirectionalClips_[i],p==s.pairs.end()?~0u:p->armsClip);
 }
 const unsigned blocks[]={s.blockStartClip,s.blockLoopClip,s.blockEndClip};
 for(int i=0;i<3;++i)populate(assemblyBlockClips_[i],blocks[i]);
 std::vector<unsigned> variants;for(const auto& p:s.pairs)if(p.action=="Attack")variants.push_back(p.armsClip);
 for(int i=0;i<4;++i)populate(assemblyAttackVariants_[i],size_t(i)<variants.size()?variants[i]:~0u);
 auto e=Euler(s.offHandWeaponRotation);
 const float values[]={s.offHandWeaponPosition.x,s.offHandWeaponPosition.y,s.offHandWeaponPosition.z,e.x,e.y,e.z,
  s.weaponScale,s.offHandWeaponScale,s.bladeBase.x,s.bladeBase.y,s.bladeBase.z,s.bladeTip.x,s.bladeTip.y,s.bladeTip.z,
  s.shieldCenter.x,s.shieldCenter.y,s.shieldCenter.z,s.shieldHalfExtents.x,s.shieldHalfExtents.y,s.shieldHalfExtents.z,
  s.bladeRadius,s.maximumHandCorrection,s.fullChargeSeconds,s.chainWindowSeconds,s.queuedReleaseSeconds};
 for(int i=0;i<25;++i)assemblyHandValues_[i].SetValue(values[i]);
 assemblyAvoidance_.SetCheck(s.avoidOffHand);
 for(size_t i=0;i<Actions.size();++i)assemblyWeaponClips_[i].SetVisible(!s.IndependentHands()&&i/6==size_t(std::max(assemblyActionPage_.GetSelected(),0)));
 ShowAssemblyHandPage(std::max(assemblyHandPage_.GetSelected(),0));
}
