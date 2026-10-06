#pragma once
// Owner asset fixture builder; the product implementation contains no pack names.
static bool SwordShieldPlayableProof(const fs::path& input,const fs::path& output)
{
 std::error_code ec;fs::copy(input,output,fs::copy_options::recursive|fs::copy_options::overwrite_existing,ec);
 if(ec){std::cerr<<ec.message()<<"\n";return false;}
 auto text=[&](const char* file){std::ifstream f(input/file);std::string value;f>>value;return value;};
 const auto projectId=text("project-id.txt"),armsId=text("arms-id.txt"),swordId=text("Sword-id.txt"),shieldId=text("Shield-id.txt");
 auto source=CreatorAssetWorkflowService().PrepareModelPlacement(output.generic_u8string(),projectId,armsId);
 if(!source.IsReady()){std::cerr<<source.Result().error<<"\n";return false;}
 auto& native=*source.PeekMutableScene();std::string error;
 std::vector<PlayerViewBoneChoice> bones;if(!CollectPlayerViewBones(native,bones,error))return false;
 auto choice=[&](const char* name)->PlayerViewBoneChoice {
  for(const auto& c:bones) {const auto* n=native.names.GetComponent(c.entity);if(n&&n->name==name)return c;}
  return {};
 };
 auto index=[&](const char* name) {
  for(size_t i=0;i<native.animations.GetCount();++i) {
   const auto* n=native.names.GetComponent(native.animations.GetEntity(i));
   if(n&&n->name==std::string("SwordShield / ")+name)return unsigned(i);
  }
  return std::numeric_limits<unsigned>::max();
 };
 Pose(native,0,"Idle");native.humanoids.Clear();native.Update(1.0f/60);
 FirstPersonAssemblySettings settings;settings.armsAssetId=armsId;settings.weaponAssetId=swordId;
 settings.offHandWeaponAssetId=shieldId;
 settings.parentBonePath=choice("hand_r").path;settings.offHandParentBonePath=choice("hand_l").path;
 settings.primaryLayerRootPath=choice("clavicle_r").path;settings.offHandLayerRootPath=choice("clavicle_l").path;
 settings.blockStartClip=index("BlockStart");settings.blockLoopClip=index("BlockLoop");settings.blockEndClip=index("BlockEnd");
 settings.pairs={{"Idle",index("Idle"),0},{"Walk",index("Walk"),0},{"Run",index("Sprint"),0},{"Attack",index("AttackLeft"),0},{"Attack",index("AttackRight"),0},{"Attack",index("AttackDown"),0},{"Attack",index("AttackStab"),0}};
 for(const char* direction:{"Left","Right","Down","Stab"})
  for(const char* stage:{"Charge","Hold","Release"}) {
   const auto source=std::string("Attack")+direction+stage;
   settings.pairs.push_back({std::string("Melee")+direction+stage,index(source.c_str()),0});
  }
 settings.cameraPosition={0,-1.65f,0.08f};settings.cameraRotation={0,0.707106781f,-0.707106781f,0};
 XMStoreFloat4(&settings.cameraRotation,XMQuaternionMultiply(XMLoadFloat4(&settings.cameraRotation),XMQuaternionRotationAxis(XMVectorSet(0,1,0,0),XM_PIDIV2)));
 // UE socket metadata retained from supplied Blueprint/Skeleton, converted to
 // the imported FBX frame, then rebound from IK helper bones onto real hands.
 const auto meshBasis=XMMatrixRotationX(-XM_PIDIV2);
 auto bind=[&](const char* hand,const char* ik,const XMFLOAT3& p,const XMFLOAT4& q,XMFLOAT3& position,XMFLOAT4& rotation) {
  const auto* ht=native.transforms.GetComponent(choice(hand).entity);
  const auto* it=native.transforms.GetComponent(choice(ik).entity);
  if(!ht||!it)return false;
  const auto relative=meshBasis*XMMatrixRotationQuaternion(XMLoadFloat4(&q))*XMMatrixTranslation(p.x,p.y,p.z)*
   XMLoadFloat4x4(&it->world)*XMMatrixInverse(nullptr,XMLoadFloat4x4(&ht->world));
  XMVECTOR scale,r,t;if(!XMMatrixDecompose(&scale,&r,&t,relative))return false;
  XMStoreFloat3(&position,t);XMStoreFloat4(&rotation,XMQuaternionNormalize(r));
  return true;
 };
 if(!bind("hand_r","ik_hand_gun",{0.04764772f,-0.03480677f,-0.84f},
   {0,0,-0.087155743f,0.996194698f},settings.weaponPosition,settings.weaponRotation) ||
   !bind("hand_l","ik_hand_l",{0.09445860f,0.04526711f,-0.00783213f},
   {0.79698964f,-0.59486753f,-0.09359146f,0.04669868f},settings.offHandWeaponPosition,settings.offHandWeaponRotation))return false;
 // The separate static sword has its origin at the guard, unlike the pack's
 // skeletal sword pivot. Place the middle of its handle inside the curled grip.
 {
  XMVECTOR grip=XMVectorZero();
  for(const char* name:{"index_01_r","middle_01_r","ring_01_r","pinky_01_r","thumb_02_r"})
   grip+=native.transforms.GetComponent(choice(name).entity)->GetPositionV();
  grip/=5.0f;
  const auto handInverse=XMMatrixInverse(nullptr,XMLoadFloat4x4(&native.transforms.GetComponent(choice("hand_r").entity)->world));
  const auto localGrip=XMVector3TransformCoord(grip,handInverse);
  const auto handle=XMVector3Rotate(XMVectorSet(0,-0.04f,0,0),XMLoadFloat4(&settings.weaponRotation));
  const auto lower=XMVector3Rotate(XMVectorSet(0,-0.02f,0,0),XMLoadFloat4(&settings.weaponRotation));
  XMStoreFloat3(&settings.weaponPosition,localGrip-handle+lower);
 }


 if(std::getenv("RENEGADE_HAND_COLLISION")) {
  settings.avoidOffHand=true;
  for(const auto& id:{swordId,shieldId}) {
   auto part=CreatorAssetWorkflowService().PrepareModelPlacement(output.generic_u8string(),projectId,id);
   if(!part.IsReady())return false;auto& ps=*part.PeekMutableScene();
   wi::ecs::Entity root=0;
   for(size_t j=0;j<ps.transforms.GetCount();++j)
    if(!ps.hierarchy.Contains(ps.transforms.GetEntity(j)))root=ps.transforms.GetEntity(j);
   const auto inverse=XMMatrixInverse(nullptr,renegade::runtime::PlayerHandWorld(ps,root));
   XMFLOAT3 minimum={100,100,100},maximum={-100,-100,-100};float bladeRadius=0;
   for(size_t j=0;j<ps.objects.GetCount();++j) {
    const auto* mesh=ps.meshes.GetComponent(ps.objects[j].meshID);if(!mesh)return false;
    const auto matrix=renegade::runtime::PlayerHandWorld(ps,ps.objects.GetEntity(j))*inverse;
    for(const auto& vertex:mesh->vertex_positions) {
     XMFLOAT3 v;XMStoreFloat3(&v,XMVector3TransformCoord(XMLoadFloat3(&vertex),matrix));
     for(int axis=0;axis<3;++axis) {(&minimum.x)[axis]=std::min((&minimum.x)[axis],(&v.x)[axis]);(&maximum.x)[axis]=std::max((&maximum.x)[axis],(&v.x)[axis]);}
     if(v.y>0.03f)bladeRadius=std::max(bladeRadius,std::sqrt(v.x*v.x+v.z*v.z));
    }
   }
   std::cout<<"ROOT PART "<<id<<" "<<minimum.x<<","<<minimum.y<<","<<minimum.z<<" .. "<<maximum.x<<","<<maximum.y<<","<<maximum.z<<"\n";
   if(id==swordId) {settings.bladeBase={0,0.03f,0};settings.bladeTip={0,maximum.y,0};settings.bladeRadius=bladeRadius+0.005f;}
   else {settings.shieldCenter={(minimum.x+maximum.x)/2,(minimum.y+maximum.y)/2,(minimum.z+maximum.z)/2};
    settings.shieldHalfExtents={(maximum.x-minimum.x)/2+0.005f,(maximum.y-minimum.y)/2+0.005f,(maximum.z-minimum.z)/2+0.005f};}
  }
 }
 FirstPersonAssemblyService service;StableId asset;
 if(!service.Save(output.generic_u8string(),projectId,"Sword Shield Test",settings,{},asset,error)){
  std::cerr<<"ASSEMBLY "<<error<<"\n";return false;
 }
 FirstPersonAssemblySettings reopenedSettings;
 if(!service.ReadSettings(output.generic_u8string(),projectId,asset,reopenedSettings,error)||!reopenedSettings.IndependentHands())return false;
 if(const char* replacement=std::getenv("RENEGADE_ASSEMBLY_SWAP")) {
  auto candidate=ModelImportCandidateService().PrepareStaticModel(replacement);
  if(!candidate.IsReady()){std::cerr<<candidate.Error()<<"\n";{std::cerr<<"AUTHORING LINE "<<__LINE__<<" "<<error<<"\n";return false;}}
  ModelImportCommitRequest request;request.projectRoot=output.generic_u8string();request.projectId=projectId;
  request.assetName="Replacement Sword";
  const auto committed=ModelImportCommitService().CommitStaticModel(request,candidate);
  if(!committed.succeeded){std::cerr<<committed.error<<"\n";{std::cerr<<"AUTHORING LINE "<<__LINE__<<" "<<error<<"\n";return false;}}
  auto edited=reopenedSettings;edited.weaponAssetId=committed.assetId;
  edited.bladeTip.y*=1.2f;
  edited.weaponScale=0.9f;edited.offHandWeaponScale=1.1f;edited.fullChargeSeconds=1.25f;
  edited.chainWindowSeconds=0.4f;edited.queuedReleaseSeconds=1.2f;
  AssetRegistry registry;if(!ReadAssetRegistry(output.generic_u8string(),projectId,registry,error)){std::cerr<<"AUTHORING LINE "<<__LINE__<<" "<<error<<"\n";return false;}
  const auto record=std::find_if(registry.records.begin(),registry.records.end(),[&](const auto& r){return r.assetId==asset;});
  if(record==registry.records.end() || !service.Update(output.generic_u8string(),projectId,asset,record->contentHash,edited,{},error)){std::cerr<<"AUTHORING LINE "<<__LINE__<<" "<<error<<"\n";return false;}
  FirstPersonAssemblySettings roundtrip;
  if(!service.ReadSettings(output.generic_u8string(),projectId,asset,roundtrip,error)||
     roundtrip.weaponAssetId!=committed.assetId||roundtrip.offHandWeaponAssetId!=shieldId||
     roundtrip.pairs.size()!=settings.pairs.size()||roundtrip.fullChargeSeconds!=1.25f||
     roundtrip.weaponScale!=0.9f||roundtrip.offHandWeaponScale!=1.1f){std::cerr<<"AUTHORING LINE "<<__LINE__<<" "<<error<<"\n";return false;}
  wi::scene::Scene swapped;if(!service.Prepare(output.generic_u8string(),projectId,roundtrip,swapped,error)){std::cerr<<"AUTHORING LINE "<<__LINE__<<" "<<error<<"\n";return false;}
  renegade::runtime::RuntimePlayerHandAnimationState hands;wi::ecs::Entity root=0;
  for(size_t i=0;i<swapped.metadatas.GetCount();++i)
   if(swapped.metadatas[i].bool_values.has("renegade.first_person.independent_hands"))root=swapped.metadatas.GetEntity(i);
  if(!renegade::runtime::InitializeRuntimePlayerHandAnimations(swapped,root,hands,error)||
     hands.fullChargeSeconds!=1.25f||hands.chainWindowSeconds!=0.4f||hands.queuedReleaseSeconds!=1.2f){std::cerr<<"AUTHORING LINE "<<__LINE__<<" "<<error<<"\n";return false;}
  for(int frame=0;frame<90;++frame) {
   renegade::runtime::UpdateRuntimePlayerHandAnimations(swapped,hands,renegade::runtime::PlayerViewAction::Idle,
    1.0f/60,false,true,true);swapped.Update(1.0f/60);
  }
  if(std::abs(hands.chargeSeconds-1.25f)>0.001f){std::cerr<<"AUTHORING LINE "<<__LINE__<<" "<<error<<"\n";return false;}
  renegade::runtime::UpdateRuntimePlayerHandAnimations(swapped,hands,renegade::runtime::PlayerViewAction::Idle,
    1.0f/60,false,true,false,true);
  if(hands.chargeStrength<0.99f){std::cerr<<"AUTHORING LINE "<<__LINE__<<" "<<error<<"\n";return false;}
  renegade::runtime::ResetRuntimePlayerHandAnimations(swapped,hands);
  renegade::studio::ModelImportPreview preview;
  if(!preview.Prepare(swapped,error)){std::cerr<<"AUTHORING LINE "<<__LINE__<<" "<<error<<"\n";return false;}preview.UseFirstPersonCamera();preview.SetAssemblyShieldHeld(true);
  for(const char* action:FirstPersonDirectionalActions)
   if(!preview.SetPairedAction(action)||!preview.Scrub(0.1f)){std::cerr<<"PREVIEW "<<action<<"\n";{std::cerr<<"AUTHORING LINE "<<__LINE__<<" "<<error<<"\n";return false;}}
  for(const char* action:{"Attack#0","Attack#1","Attack#2","Attack#3","BlockStart","BlockLoop","BlockEnd"})
   if(!preview.SetPairedAction(action)||!preview.Scrub(0.1f)){std::cerr<<"AUTHORING LINE "<<__LINE__<<" "<<error<<"\n";return false;}
  preview.SetPairedAction("MeleeRightRelease");preview.Scrub(0.12f);
  for(size_t i=0;i<preview.scene->names.GetCount();++i) {
   const auto& name=preview.scene->names[i].name;
   if(name=="hand_l"||name=="hand_r"||name==CreatorAuthoredTransformRootName) {
    const auto* t=preview.scene->transforms.GetComponent(preview.scene->names.GetEntity(i));
    if(t){const auto p=t->GetPosition();std::cout<<"PREVIEW "<<name<<" "<<p.x<<","<<p.y<<","<<p.z<<"\n";}
   }
  }
  for(int frame=0;frame<3000;++frame){wi::eventhandler::FireEvent(wi::eventhandler::EVENT_THREAD_SAFE_POINT,0);preview.PreUpdate();preview.Update(1.0f/60);preview.PreRender();preview.Render();wi::graphics::GetDevice()->SubmitCommandLists();wi::renderer::UpdateGPUSuballocator();Sleep(10);if(preview.IsReady())break;}
  std::vector<std::uint8_t> png;
  if(!preview.CapturePng(png,error)){std::cerr<<"AUTHORING LINE "<<__LINE__<<" "<<error<<"\n";return false;}
  const auto texture=wi::resourcemanager::Load("replacement-proof-"+GenerateStableId()+".png",wi::resourcemanager::Flags::NONE,png.data(),png.size());
  wi::vector<std::uint8_t> pixels;
  if(!texture.IsValid()||!wi::helper::saveTextureToMemory(texture.GetTexture(),pixels)||pixels.size()<512*320*4)return false;
  size_t contrasting=0;
  for(size_t p=0;p+4<=512*320*4;p+=4)
   contrasting+=std::abs(int(pixels[p])-int(pixels[0]))+std::abs(int(pixels[p+1])-int(pixels[1]))+std::abs(int(pixels[p+2])-int(pixels[2]))>20;
  if(contrasting<50){std::cerr<<"Replacement preview is empty\n";return false;}
  std::ofstream image(output/"replacement-sword-held-shield.png",std::ios::binary);
  image.write(reinterpret_cast<const char*>(png.data()),png.size());image.close();
  // Restore defaults on the original assembly; retain an authored copy for native UI.
  StableId editedAsset;
  if(!service.Save(output.generic_u8string(),projectId,"Authored Replacement Sword",roundtrip,png,editedAsset,error)){std::cerr<<"AUTHORING LINE "<<__LINE__<<" "<<error<<"\n";return false;}
  if(!ReadAssetRegistry(output.generic_u8string(),projectId,registry,error)){std::cerr<<"AUTHORING LINE "<<__LINE__<<" "<<error<<"\n";return false;}
  const auto latest=std::find_if(registry.records.begin(),registry.records.end(),[&](const auto& r){return r.assetId==asset;});
  if(latest==registry.records.end()||!service.Update(output.generic_u8string(),projectId,asset,latest->contentHash,settings,{},error)){std::cerr<<"AUTHORING LINE "<<__LINE__<<" "<<error<<"\n";return false;}
  std::cout<<"ASSEMBLY AUTHORING PASS: different sword mesh; scale; timing; update/reopen; all masked preview slots\n";
 }
 wi::scene::Scene assembly;if(!service.Prepare(output.generic_u8string(),projectId,reopenedSettings,assembly,error))return false;
 for(size_t i=0;i<assembly.materials.GetCount();++i) {
  assembly.materials[i].baseColor=XMFLOAT4(0.7f,0.75f,0.8f,1);assembly.materials[i].metalness=0;assembly.materials[i].roughness=0.8f;
 }
 if(!Capture(assembly,output/"sword-shield-idle-model.png"))return false;
 renegade::runtime::RuntimePlayerViewRigState rig;
 for(size_t i=0;i<assembly.metadatas.GetCount();++i)
  if(assembly.metadatas[i].bool_values.has("renegade.first_person.independent_hands"))
   rig.viewModelRoot=assembly.metadatas.GetEntity(i);
 renegade::runtime::RuntimePlayerViewAnimationState animation;
 if(!renegade::runtime::InitializeRuntimePlayerViewAnimations(assembly,rig,animation,error)||!animation.handLayers.enabled){
  std::cerr<<"HAND INIT "<<error<<"\n";return false;
 }
 if(!animation.handLayers.directional)return false;
 const auto step=[&](float dt,bool charge,bool release,float yaw=0,float pitch=0,bool cancel=false) {
  const auto unresolved=animation.handLayers.avoidance.unresolved;
  renegade::runtime::UpdateRuntimePlayerViewAnimations(assembly,animation,renegade::runtime::PlayerViewAction::Idle,
   dt,false,false,false,false,true,charge,release,true,yaw,pitch,cancel);
  assembly.Update(dt);
  if(animation.handLayers.avoidance.unresolved!=unresolved)std::cerr<<"CONTACT direction="<<unsigned(animation.handLayers.direction)<<" time="<<animation.handLayers.rightTime<<" residual="<<animation.handLayers.avoidance.residual<<"\n";
 };
 for(int frame=0;frame<80;++frame)step(1.0f/60,false,false);

 // Capture already evaluated poses without letting preview redraws repeatedly
 // apply a fractional amount and converge to the destination.
 const auto capturePose=[&](const std::string& name) {
  std::vector<float> amounts;
  for(size_t i=0;i<assembly.animations.GetCount();++i) {
   amounts.push_back(assembly.animations[i].amount);assembly.animations[i].amount=0;
  }
  const bool ok=Capture(assembly,output/name);
  for(size_t i=0;i<amounts.size();++i)assembly.animations[i].amount=amounts[i];
  return ok;
 };
 const auto rightAmount=[&]() {
  auto& h=animation.handLayers;
  return (h.avoidance.enabled?h.avoidance.pose.get():&assembly)->animations.GetComponent(h.right)->amount;
 };
 std::set<wi::ecs::Entity> played;
 for(unsigned direction=0;direction<4;++direction) {
  const float yaw=direction==0?-0.1f:direction==1?0.1f:0;
  const float pitch=direction==2?0.1f:direction==3?-0.1f:0;
  step(1.0f/60,true,false,yaw,pitch);
  if(animation.handLayers.direction!=direction || animation.handLayers.attacking)return false;
  const auto before=animation.handLayers.chargeSeconds;
  step(0,true,false,-yaw,-pitch);
  if(animation.handLayers.chargeSeconds!=before || animation.handLayers.direction!=direction)return false;
  bool chargeFade=false,holdFade=false;
  for(int frame=0;frame<90;++frame) {
   step(1.0f/60,true,false);
   const float weight=rightAmount();
   if(weight>0 && weight<1) {
    if(animation.handLayers.chargePhase==1)chargeFade=true;
    if(animation.handLayers.chargePhase==2)holdFade=true;
    if(direction==3 && frame==2 && !capturePose("stab-blend-charge.png"))return false;
   }
  }
  if(!chargeFade||!holdFade)return false;
  if(animation.handLayers.chargePhase!=2 || animation.handLayers.chargeSeconds<0.99f)return false;
  if(!Capture(assembly,output/("sword-charge-"+std::to_string(direction)+".png")))return false;
  step(1.0f/60,false,true);
  if(!animation.handLayers.attacking || animation.handLayers.chargeStrength<0.99f)return false;
  played.insert(animation.handLayers.right);
  for(int frame=0;frame<180;++frame) {
   step(1.0f/60,false,false);
   if(direction==3 && frame==1) {
    const auto weight=rightAmount();
    if(!(weight>0 && weight<1)||!capturePose("stab-blend-release.png"))return false;
   }
   if(frame==12 && !Capture(assembly,output/("sword-release-"+std::to_string(direction)+".png")))return false;
  }
  if(animation.handLayers.blockPhase!=2 || animation.handLayers.attacking)return false;
 }
 if(played.size()!=4)return false;
 // A quick release carries low strength; cancel clears windup without a strike.
 step(1.0f/60,true,false);step(1.0f/60,false,true);
 if(!animation.handLayers.attacking || animation.handLayers.chargeStrength>0.1f)return false;
 for(int frame=0;frame<180;++frame)step(1.0f/60,false,false);
 step(1.0f/60,true,false);step(1.0f/60,false,false,0,0,true);
 if(animation.handLayers.attacking || animation.handLayers.chargePhase!=0)return false;

 step(1.0f/60,true,false,-0.1f,0);
 for(int frame=0;frame<2;++frame)step(1.0f/60,true,false);
 step(1.0f/60,true,false,0.1f,0);
 if(animation.handLayers.direction!=1 || animation.handLayers.leftBlend.clip!=animation.handLayers.left)return false;
 step(1.0f/60,false,false,0,0,true);
 for(int frame=0;frame<20;++frame)step(1.0f/60,false,false);
 if(!capturePose("blend-cancel-idle.png"))return false;

 if(animation.handLayers.avoidance.enabled) {
  const auto& collision=animation.handLayers.avoidance;
  std::cout<<"AVOIDANCE corrected_frames="<<collision.corrections<<" unresolved_frames="<<collision.unresolved<<"\n";
  if(!collision.corrections||collision.unresolved)return false;
 }
 renegade::runtime::ResetRuntimePlayerViewAnimations(assembly,animation);
 EquipmentDefinition sword;sword.assetId=GenerateStableId();sword.name="Sword Test";
 sword.handUse=EquipmentHandUse::PrimaryOnly;sword.presentationAssetId=asset;
 sword.actions.push_back({EquipmentAction::Charge,"Charge",0,0,0,0,true});
 sword.actions.push_back({EquipmentAction::Release,"Release",0,0,0,0.15f});
 auto swordSaved=SaveEquipmentAsset(output.generic_u8string(),projectId,sword);
 if(!swordSaved.succeeded){std::cerr<<swordSaved.error<<"\n";return false;}
 EquipmentDefinition shield;shield.assetId=GenerateStableId();shield.name="Shield Test";
 shield.handUse=EquipmentHandUse::OffHandOnly;shield.presentationAssetId=asset;
 EquipmentActionDefinition block;block.action=EquipmentAction::Block;block.animationAction="Block";block.activeWhileHeld=true;
 block.recoverySeconds=native.animations[index("BlockEnd")].end-native.animations[index("BlockEnd")].start;
 shield.actions.push_back(block);
 auto shieldSaved=SaveEquipmentAsset(output.generic_u8string(),projectId,shield);
 if(!shieldSaved.succeeded){std::cerr<<shieldSaved.error<<"\n";return false;}
 wi::scene::Scene level;
 const auto floor=level.Entity_CreateCube("Sword shield test floor");
 auto& floorTransform=*level.transforms.GetComponent(floor);floorTransform.Scale(XMFLOAT3(12,0.5f,12));floorTransform.Translate(XMFLOAT3(0,-0.5f,0));floorTransform.UpdateTransform();
 level.materials.GetComponent(floor)->baseColor=XMFLOAT4(0.22f,0.3f,0.22f,1);
 auto& body=level.rigidbodies.Create(floor);body.mass=0;body.shape=wi::scene::RigidBodyPhysicsComponent::CollisionShape::BOX;body.box.halfextents={12,0.5f,12};
 auto& weather=level.weathers.Create(wi::ecs::CreateEntity());weather.ambient={0.4f,0.4f,0.4f};weather.horizon={0.35f,0.5f,0.7f};weather.zenith={0.1f,0.25f,0.5f};
 const auto light=level.Entity_CreateLight("Test sun",XMFLOAT3(0,5,0),XMFLOAT3(0.9f,0.85f,0.7f),4);
 level.lights.GetComponent(light)->type=wi::scene::LightComponent::DIRECTIONAL;
 CreatePlayerStartCommand create(level,{});if(!create.Execute())return false;
 auto playerSettings=CapturePlayerControllerSettings(level,create.CreatedEntity());
 playerSettings.firstPersonArmsAssetId=asset;playerSettings.primaryEquipmentAssetId=swordSaved.document.equipment.assetId;
 playerSettings.offHandEquipmentAssetId=shieldSaved.document.equipment.assetId;
 if(!SetPlayerControllerSettingsCommand(level,create.CreatedEntity(),playerSettings).Execute())return false;
 fs::create_directories(output/"Content/Scenes");
 const auto scenePath=output/"Content/Scenes/SwordShieldTest.wiscene";
 wi::Archive archive(scenePath.generic_u8string(),false,false);level.Serialize(archive);
 if(!archive.SaveFile(scenePath.generic_u8string()))return false;archive=wi::Archive();
 SceneService verify;if(!verify.LoadScene(scenePath.generic_u8string())||
  CapturePlayerControllerSettings(verify.GetScene(),ResolvePlayerStart(verify.GetScene()).start.entity).offHandEquipmentAssetId!=shieldSaved.document.equipment.assetId)return false;
 std::ofstream descriptor(output/"SwordShieldTest.renegade");
 descriptor<<"format = renegade-project\nversion = 1\n\n[project]\nproject_id = "<<projectId<<"\nname = Sword Shield Test\nstartup_scene = Content/Scenes/SwordShieldTest.wiscene\n";
 descriptor.close();
 ProjectMetadata project;
 if(!ProjectService().InspectProject((output/"SwordShieldTest.renegade").generic_u8string(),project,error))return false;
 CommandService commands;TestLevelSnapshotService snapshots(verify,commands);TestLevelSnapshot snapshot;
 if(!snapshots.Create(project,snapshot,error)){std::cerr<<"SNAPSHOT "<<error<<"\n";return false;}
 EquipmentAssetDocument loadedShield;
 if(!LoadEquipmentAsset(snapshot.sessionDirectory,projectId,shieldSaved.document.equipment.assetId,loadedShield,error))return false;
 auto snapshotAssembly=ReusableAssetService().PrepareModelAssetPlacement({snapshot.sessionDirectory,projectId,asset});
 if(!snapshotAssembly.IsReady())return false;
 std::ofstream(output/"test-level-descriptor.txt")<<snapshot.descriptorPath;
 std::ofstream(output/"assembly-id.txt")<<asset;
 std::cout<<"PLAYABLE SWORD SHIELD SAVED // "<<(output/"SwordShieldTest.renegade").generic_u8string()<<"\n";
 return true;
}
