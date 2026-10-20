#pragma once
#include "../Runtime/src/RuntimeBurningStatus.h"
#include "../Runtime/src/RuntimeChannelVisual.h"
#include "../Runtime/src/RuntimeProjectileVisuals.h"
// Manual proof for private owner assets. No supplied assets are bundled.
static bool MagicPoseCapture(wi::scene::Scene& scene,unsigned index,float seconds,const fs::path& path) {
    if(index>=scene.animations.GetCount())return false;
    for(size_t i=0;i<scene.animations.GetCount();++i) {
        auto& clip=scene.animations[i];clip.Pause();clip.RootMotionOff();clip.SetLooped(false);
        clip.amount=i==index?1.f:0.f;clip.timer=clip.start+std::min(seconds,clip.end-clip.start);
        clip.last_update_time=-std::numeric_limits<float>::max();
    }
    scene.Update(1.f/60);
    return Capture(scene,path);
}
static bool MagicAssetReopenProof(const fs::path& project,const fs::path& output) {
    nlohmann::json j;std::ifstream(project/"magic-assets.json")>>j;
    const auto root=project.generic_u8string();const std::string projectId=j.at("projectId"),assetId=j.at("armsId");
    auto placed=CreatorAssetWorkflowService().PrepareModelPlacement(root,projectId,assetId);
    if(!placed.IsReady()){std::cerr<<placed.Result().error<<'\n';return false;}
    auto& scene=*placed.PeekMutableScene();
    const auto& rows=j.at("clips");
    if(scene.animations.GetCount()!=rows.size())return false;
    for(const auto& row:rows) {
        const unsigned index=row.at("index");const float duration=row.at("duration");
        if(std::abs(scene.animations[index].end-scene.animations[index].start-duration)>.001f)return false;
        if(!MagicPoseCapture(scene,index,duration*.5f,output/(row.at("label").get<std::string>()+".png")))return false;
    }
    std::cout<<"MAGIC GOVERNED ARMS COLD REOPEN AND NATIVE CLIP RENDERS PASS\n";return true;
}
static bool MagicAssetProof(const fs::path& input,const fs::path& project) {
    if(fs::exists(project/AssetRegistryDocumentName)){std::cerr<<"Use a fresh disposable project\n";return false;}
    SceneService blank;wi::Archive starter;blank.GetScene().Serialize(starter);
    const auto templatePath=fs::absolute(project.parent_path()/(project.filename().generic_u8string()+"-template.wiscene"));
    if(!starter.SaveFile(templatePath.generic_u8string()))return false;
    ProjectService projects;
    if(!projects.CreateProject(fs::absolute(project.parent_path()).generic_u8string(),project.filename().generic_u8string(),templatePath.generic_u8string())){
        std::cerr<<projects.LastError()<<'\n';return false;
    }
    const auto projectId=projects.CurrentProject().projectId;std::string error;
    auto candidate=ModelImportCandidateService().PrepareModel((input/"Y_Idle.FBX").generic_u8string());
    if(!candidate.IsReady()){std::cerr<<candidate.Error()<<'\n';return false;}
    if(candidate.PeekScene()->animations.GetCount()!=1)return false;
    const auto before=candidate.Evidence();std::vector<std::string> actions={"Idle"};
    nlohmann::json rows=nlohmann::json::array();
    const auto addRow=[&](unsigned index,const std::string& label) {
        const auto& clip=candidate.PeekScene()->animations[index];
        rows.push_back({{"index",index},{"label",label},{"duration",clip.end-clip.start}});
    };
    addRow(0,"Idle");
    const std::vector<std::tuple<const char*,const char*,const char*>> clips={
        {"Y_Walk.FBX","Walk","Walk"},{"Y_Sprint.FBX","Run","Sprint"},
        {"Y_Equip.FBX","Unassigned","Equip"},{"Y_Unequip.FBX","Unassigned","Unequip"},
        {"Y_R_Projectile_Start.FBX","Unassigned","ProjectileStart"},
        {"Y_R_Projectile_Loop.FBX","Unassigned","ProjectileLoop"},
        {"Y_R_Projectile_End.FBX","Unassigned","ProjectileEnd"},
        {"Y_R_Beam_Start.FBX","Unassigned","BeamStart"},
        {"Y_R_Beam_Loop.FBX","Unassigned","BeamLoop"},
        {"Y_R_Beam_End.FBX","Unassigned","BeamEnd"}};
    for(const auto& entry:clips) {
        const unsigned first=unsigned(candidate.PeekScene()->animations.GetCount());
        if(!ModelImportCandidateService().AppendExternalAnimations(candidate,(input/std::get<0>(entry)).generic_u8string(),error)){
            std::cerr<<"MAGIC APPEND "<<std::get<0>(entry)<<": "<<error<<'\n';return false;
        }
        if(candidate.PeekScene()->animations.GetCount()!=first+1)return false;
        candidate.PeekMutableScene()->names.Create(candidate.PeekScene()->animations.GetEntity(first)).name=std::string("Mage / ")+std::get<2>(entry);
        actions.push_back(std::get<1>(entry));addRow(first,std::get<2>(entry));
    }
    if(candidate.Evidence().skinIndexFingerprint!=before.skinIndexFingerprint ||
       candidate.Evidence().inverseBindFingerprint!=before.inverseBindFingerprint ||
       std::any_of(candidate.ExternalAnimations().begin(),candidate.ExternalAnimations().end(),[](const auto& clip){return !clip.matchingRig;}))return false;
    ModelImportCommitRequest request;request.projectRoot=project.generic_u8string();request.projectId=projectId;
    request.assetName="Mage Arms Reference";request.characterAsset=true;request.playerRole="arms";
    request.animationActions=actions;request.destinationFolder="Content/Arms";
    const auto saved=ModelImportCommitService().CommitModel(request,candidate);
    if(!saved.succeeded){std::cerr<<saved.error<<'\n';return false;}
    std::ofstream(project/"magic-assets.json")<<nlohmann::json{{"projectId",projectId},{"armsId",saved.assetId},
        {"clips",rows},{"status","native arms import proof; ability lifecycle and effects not implemented"}}.dump(2);
    ReusableModelAssetDocument document;CreatorModelImportRecipe recipe;
    if(!ReadReusableModelAssetDocument((project/fs::u8path(saved.assetProjectRelativePath)).generic_u8string(),document,error) ||
       !ParseCreatorModelImportOptions(nlohmann::json::parse(document.manifest.settingsJson).at("options").dump(),recipe,error))return false;
    wi::scene::Scene rebuilt;ImportModel_FBX((project/fs::u8path(saved.sourceProjectRelativePath)).generic_u8string(),rebuilt);
    if(!ApplyCreatorModelImportRecipe(rebuilt,request.projectRoot,projectId,recipe,error) || rebuilt.animations.GetCount()!=actions.size()){
        std::cerr<<"MAGIC RETAINED REBUILD "<<error<<'\n';return false;
    }
    std::cout<<"MAGIC ORIGINAL RIG MATCH, SKIN/BIND PRESERVATION, GOVERNED SAVE AND RECIPE REBUILD PASS\n";
    return MagicAssetReopenProof(project,project);
}

// Optional private governed Character asset; source project remains untouched.
static bool MagicMutantTarget(const fs::path& project,wi::scene::Scene& world) {
    nlohmann::json fixture;std::ifstream(project/"burning-target.json")>>fixture;
    ProjectService projects;ProjectMetadata source;std::string error;
    if(!projects.InspectProject(fixture.at("sourceDescriptor").get<std::string>(),source,error))return false;
    auto prepared=CreatorAssetWorkflowService().PrepareModelPlacement(source.rootPath,source.projectId,fixture.at("assetId"));
    if(!prepared.IsReady()){std::cerr<<prepared.Result().error<<std::endl;return false;}
    auto& rig=*prepared.PeekMutableScene();
    std::cout<<"SOURCE PHYSICS bodies="<<rig.rigidbodies.GetCount()<<" colliders="<<rig.colliders.GetCount()<<std::endl;
    for(size_t i=0;i<rig.rigidbodies.GetCount();++i) {
        const auto& b=rig.rigidbodies[i];
        std::cout<<"SOURCE BODY shape="<<int(b.shape)<<" box="<<b.box.halfextents.x<<","<<b.box.halfextents.y<<","<<b.box.halfextents.z<<std::endl;
    }
    rig.rigidbodies.Clear();rig.colliders.Clear();
    for(size_t i=0;i<rig.humanoids.GetCount();++i) {
        std::cout<<"SOURCE HUMANOID ragdoll="<<rig.humanoids[i].IsRagdollPhysicsEnabled()<<std::endl;
        rig.humanoids[i].SetRagdollDisabled(true);rig.humanoids[i].SetRagdollPhysicsEnabled(false);rig.humanoids[i].SetLookAtEnabled(false);
    }
    // The governed payload retains all retarget/source keyframe dependencies.
    for(const auto entity:CollectCharacters(rig)) {
        RemoveCharacterCommand remove(rig,entity);if(!remove.Execute())return false;
    }
    for(size_t i=0;i<rig.materials.GetCount();++i)for(auto& texture:rig.materials[i].textures)
        if(!texture.resource.IsValid())texture={};
    for(size_t i=0;i<rig.animations.GetCount();++i) {
        auto& clip=rig.animations[i];clip.Stop();clip.RootMotionOff();
        const auto* n=rig.names.GetComponent(rig.animations.GetEntity(i));
        if(!n)continue;
        auto name=n->name;std::transform(name.begin(),name.end(),name.begin(),[](unsigned char c){return char(std::tolower(c));});
        const char* action=name.find("dying")!=std::string::npos?"Death":
            name.find("idle")!=std::string::npos || name.find("flexing")!=std::string::npos?"Idle":
            name.find("walking")!=std::string::npos?"Walk":name.find("run")!=std::string::npos?"Run":
            name.find("swiping")!=std::string::npos?"Attack":"Unassigned";
        rig.metadatas.Create(rig.animations.GetEntity(i)).string_values.set(CreatorCharacterAnimationActionMetadataKey,action);
        std::cout<<"MUTANT CLIP "<<n->name<<" action="<<action<<std::endl;
    }
    wi::vector<wi::ecs::Entity> roots;
    for(size_t i=0;i<rig.transforms.GetCount();++i)if(!rig.hierarchy.Contains(rig.transforms.GetEntity(i)))
        roots.push_back(rig.transforms.GetEntity(i));
    const auto target=rig.Entity_CreateTransform("Burning mutant");
    for(const auto entity:roots)rig.Component_Attach(entity,target,true);
    rig.transforms.GetComponent(target)->Translate(XMFLOAT3{0,0,8});rig.transforms.GetComponent(target)->UpdateTransform();
    CharacterAuthoringSettings character;character.autonomous=false;character.factionId="Neutral";
    MakeCharacterCommand promote(rig,target,character);if(!promote.Execute())return false;
    for(size_t i=0;i<rig.objects.GetCount();++i)
        rig.objects[i].filterMask &= ~wi::enums::FILTER_NAVIGATION_MESH;
    world.Instantiate(rig,false);
    std::cout<<"MUTANT GOVERNED NATIVE PAYLOAD COPIED"<<std::endl;
    return true;
}

static bool MagicPlayableProof(const fs::path& project,const fs::path& output) {
    using namespace renegade::runtime;
    nlohmann::json ids;std::ifstream(project/"magic-assets.json")>>ids;
    const std::string projectId=ids.at("projectId"),armsId=ids.at("armsId");std::string error;
    auto placed=CreatorAssetWorkflowService().PrepareModelPlacement(project.generic_u8string(),projectId,armsId);
    if(!placed.IsReady())return false;auto& arms=*placed.PeekMutableScene();
    if(!MagicPoseCapture(arms,0,0,output/"source-idle.png"))return false;
    std::vector<PlayerViewBoneChoice> bones;if(!CollectPlayerViewBones(arms,bones,error))return false;
    const auto bone=[&](const char* name)->const PlayerViewBoneChoice* {
        const auto it=std::find_if(bones.begin(),bones.end(),[&](const auto& b){return nlohmann::json::parse(b.path).back()==name;});
        return it==bones.end()?nullptr:&*it;
    };
    const auto right=bone("hand_r"),left=bone("hand_l"),head=bone("head"),pelvis=bone("pelvis");
    if(!right||!left||!head||!pelvis)return false;
    const auto pos=[&](const PlayerViewBoneChoice* b){return XMLoadFloat4x4(&arms.transforms.GetComponent(b->entity)->world).r[3];};
    // Derive the view basis from the source rest/idle anatomy, avoiding filename
    // axis assumptions. Camera height remains separate from player world ownership.
    const auto hp=pos(head),up=XMVector3Normalize(hp-pos(pelvis));
    auto forward=(pos(right)+pos(left))*.5f-hp;
    forward=XMVector3Normalize(forward-up*XMVector3Dot(forward,up));
    const auto lateral=XMVector3Normalize(XMVector3Cross(up,forward));
    XMFLOAT3 r,u,f;XMStoreFloat3(&r,lateral);XMStoreFloat3(&u,up);XMStoreFloat3(&f,forward);
    const auto basis=XMMatrixSet(r.x,u.x,f.x,0,r.y,u.y,f.y,0,r.z,u.z,f.z,0,0,0,0,1);
    FirstPersonAssemblySettings settings;settings.armsAssetId=armsId;settings.spellPresentation=true;
    settings.parentBonePath=right->path;settings.spellHoldClip=6;
    XMStoreFloat4(&settings.cameraRotation,XMQuaternionRotationMatrix(basis));
    XMStoreFloat3(&settings.cameraPosition,-XMVector3TransformCoord(hp+up*.08f,basis));
    settings.pairs={{"Idle",0,0},{"Walk",1,0},{"Run",2,0},{"Equip",3,0},{"Unequip",4,0},{"Charge",5,0},{"Release",7,0}};
    LaunchSocketDefinition socket;socket.name="Right Palm";socket.part=LaunchSocketPart::Arms;
    socket.parentPath=right->path;
    // Calibrate the casting space from the palm anatomy. Distal finger tips
    // curl independently and must not define the fireball's visual centre.
    if(!MagicPoseCapture(arms,6,0,output/"source-hold-socket.png"))return false;
    const auto index=bone("index_01_r"),middle=bone("middle_01_r"),little=bone("pinky_01_r");
    if(!index||!middle||!little)return false;
    const auto knuckles=(pos(index)+pos(middle)+pos(little))/3;
    const auto palm=knuckles;
    const auto towardCamera=XMVector3Normalize(hp+up*.08f-palm);
    const auto palmCenter=palm;
    XMStoreFloat3(&socket.position,XMVector3TransformCoord(palmCenter,
        XMMatrixInverse(nullptr,arms.transforms.GetComponent(right->entity)->GetWorldMatrix())));
    std::cout<<"PALM OFFSET "<<socket.position.x<<","<<socket.position.y<<","<<socket.position.z<<"\n";
    settings.launchSockets={socket};
    FirstPersonAssemblyService service;wi::scene::Scene view;
    if(!service.Prepare(project.generic_u8string(),projectId,settings,view,error)){std::cerr<<error<<'\n';return false;}
    for(const auto& sample:std::vector<std::pair<std::string,float>>{{"Idle",0},{"Charge",.5f},{"Release",.3f}}) {
        if(!service.Pose(view,sample.first,sample.second,error)||!Capture(view,output/("fp-"+sample.first+".png"),true,true))return false;
    }
    StableId assembly;
    if(!service.Save(project.generic_u8string(),projectId,"Mage Casting "+GenerateStableId().substr(0,8),settings,{},assembly,error)){
        std::cerr<<error<<'\n';return false;
    }
    FirstPersonAssemblySettings reopened;
    if(!service.ReadSettings(project.generic_u8string(),projectId,assembly,reopened,error)||!reopened.spellPresentation||reopened.spellHoldClip!=6)return false;
    auto projectile=MakeProjectilePreset(ProjectilePreset::Spell);projectile.name="Fireball Casting Reference";
    projectile.fireballPresentation=true;
    projectile.speedMetresPerSecond=18;projectile.gravityScale=0;projectile.damage=30;
    projectile.fullChargeSeconds=2.5f;projectile.minimumChargeDamage=10;projectile.scorchOnImpact=true;
    projectile.burning={5,4};projectile.minimumChargeBurning={2,2};
    projectile.flightEffects={{ProjectileEffectKind::Flame,{},.18f,90,.25f},{ProjectileEffectKind::Smoke,{},.12f,35,.65f}};
    projectile.impactEffect=ProjectileEffectKind::Flame;
    const auto savedProjectile=SaveProjectileAsset(project.generic_u8string(),projectId,projectile);
    if(!savedProjectile.succeeded){std::cerr<<savedProjectile.error<<'\n';return false;}
    EquipmentDefinition ability;
    if(!PrepareEquipmentFromAssembly(project.generic_u8string(),projectId,assembly,"Charged Fireball",EquipmentHandUse::PrimaryOnly,ability,error)){
        std::cerr<<error<<'\n';return false;
    }
    ability.projectiles={{EquipmentAction::Release,savedProjectile.document.assetId,"Right Palm",.20f}};
    const auto savedAbility=SaveEquipmentAsset(project.generic_u8string(),projectId,ability);
    if(!savedAbility.succeeded){std::cerr<<savedAbility.error<<'\n';return false;}
    wi::scene::Scene native;
    if(!service.Prepare(project.generic_u8string(),projectId,reopened,native,error))return false;
    RuntimePlayerViewRigState rig;
    for(size_t i=0;i<native.metadatas.GetCount();++i)if(native.metadatas[i].bool_values.has("renegade.first_person.spell"))rig.viewModelRoot=native.metadatas.GetEntity(i);
    RuntimePlayerViewAnimationState animation;
    if(!InitializeRuntimePlayerViewAnimations(native,rig,animation,error)){std::cerr<<error<<'\n';return false;}
    RuntimeEquipmentLoadout loadout;PlayerControllerSettings player;player.primaryEquipmentAssetId=savedAbility.document.equipment.assetId;
    if(!loadout.Load(project.generic_u8string(),projectId,player))return false;
    const auto tick=[&](GameplayInputFrame input,float dt) {
        auto routed=loadout.RouteStaged(input,animation.equipped,animation.oneShotPlaying,dt, false,true);
        UpdateRuntimePlayerViewAnimations(native,animation,PlayerViewAction::Idle,dt,routed.firePressed,routed.reloadPressed,false,routed.toggleEquipmentPressed,true,loadout.chargePresentation,loadout.releasePresentation,false,0,0,input.cancelEquipmentPressed);
        loadout.AfterNativePresentation(animation);native.Update(1.f/60);
        for(auto& request:loadout.TakeProjectileRequests())if(animation.spellReleaseAccepted)loadout.ScheduleProjectile(std::move(request),animation.activeClip);
        return loadout.ReleaseProjectiles(animation.activeClip,animation.pairedTime,!animation.equipped);
    };
    GameplayInputFrame hold,quiet,cancel;hold.firePressed=hold.fireDown=true;cancel.cancelEquipmentPressed=true;
    tick(hold,.1f);hold.firePressed=false;for(int i=0;i<20;++i)tick(hold,.1f);
    if(animation.spellPhase!=2 || animation.loadedShells!=0 || animation.activeClip!=animation.spellHoldClip)return false;
    if(!Capture(native,output/"fp-Hold.png",true,true))return false;
    auto shots=tick(quiet,.1f);auto second=tick(quiet,.1f);shots.insert(shots.end(),second.begin(),second.end());
    if(shots.size()!=1 || shots[0].launchSocketName!="Right Palm" || animation.loadedShells!=0 ||
       std::abs(shots[0].projectile.damage-26.8f)>.01f || !shots[0].projectile.scorchOnImpact)return false;
    for(int i=0;i<20;++i)if(!tick(quiet,.1f).empty())return false;
    hold.firePressed=true;tick(hold,.1f);tick(cancel,.1f);
    if(animation.spellPhase!=0 || animation.oneShotPlaying || !tick(quiet,.1f).empty())return false;
    SceneService level;auto& world=level.GetScene();
    auto floor=world.Entity_CreateCube("Casting floor");auto& ft=*world.transforms.GetComponent(floor);ft.Scale(XMFLOAT3{30,.1f,30});ft.Translate(XMFLOAT3{0,-.1f,0});ft.UpdateTransform();
    world.objects.GetComponent(floor)->filterMask |= wi::enums::FILTER_NAVIGATION_MESH;
    auto& floorBody=world.rigidbodies.Create(floor);floorBody.mass=0;
    floorBody.shape=wi::scene::RigidBodyPhysicsComponent::CollisionShape::BOX;floorBody.box.halfextents={1,1,1};
    auto stone=world.Entity_CreateCube("Scorch target");auto& st=*world.transforms.GetComponent(stone);
    st.Scale(XMFLOAT3{1,1,1});st.Translate(XMFLOAT3{-3,1,8});st.UpdateTransform();
    const bool mutant=fs::exists(project/"burning-target.json");
    if(mutant && !MagicMutantTarget(project,world))return false;
    // Private reference targets use ordinary governed Characters.
    for(int i=mutant?1:0;i<2;++i) {
        const auto target=world.Entity_CreateTransform(i?"Fire immune dummy":"Burning dummy");
        world.transforms.GetComponent(target)->Translate(XMFLOAT3{float(i)*3,0,8});
        world.transforms.GetComponent(target)->UpdateTransform();
        CharacterAuthoringSettings character;character.autonomous=false;character.factionId="Neutral";
        MakeCharacterCommand promote(world,target,character);
        if(!promote.Execute())return false;
        if(i)world.metadatas.GetComponent(target)->bool_values.set(RuntimeBurningStatus::ImmuneKey,true);
        const auto part=[&](const char* name,XMFLOAT3 scale,XMFLOAT3 position) {
            const auto entity=world.Entity_CreateCube(name);
            auto& t=*world.transforms.GetComponent(entity);t.Scale(scale);t.Translate(position);t.UpdateTransform();
            world.Component_Attach(entity,target,true);
        };
        part("Dummy torso",{.28f,.38f,.15f},{0,1.18f,0});
        part("Dummy head",{.17f,.17f,.17f},{0,1.73f,0});
        part("Dummy left leg",{.10f,.38f,.10f},{-.16f,.43f,0});
        part("Dummy right leg",{.10f,.38f,.10f},{.16f,.43f,0});
        part("Dummy left arm",{.09f,.33f,.09f},{-.39f,1.13f,0});
        part("Dummy right arm",{.09f,.33f,.09f},{.39f,1.13f,0});
    }
    const auto sun=world.Entity_CreateLight("Casting sun",XMFLOAT3{0,5,0},XMFLOAT3{1,1,1},3,10,wi::scene::LightComponent::DIRECTIONAL);
    (void)sun;world.weathers.Create(wi::ecs::CreateEntity());
    CreatePlayerStartCommand start(world,{});if(!start.Execute())return false;
    player.firstPersonArmsAssetId=assembly;
    SetPlayerControllerSettingsCommand assign(world,start.CreatedEntity(),player);if(!assign.Execute())return false;
    for(const auto& c:CollectCharacters(world))SyncNativeCharacterPoseFromTransform(world,c);
    world.Update(0);
    for(size_t i=0;i<world.characters.GetCount();++i) {
        const auto& c=world.characters[i];std::cout<<"FIXTURE CHARACTER width="<<c.width<<" height="<<c.height<<" scale="<<c.scale<<std::endl;
    }
    for(size_t i=0;i<world.objects.GetCount();++i) {
        const auto entity=world.objects.GetEntity(i);const auto* n=world.names.GetComponent(entity);
        const auto* t=world.transforms.GetComponent(entity);const auto pos=t?t->GetPosition():XMFLOAT3{};
        std::cout<<"FIXTURE OBJECT "<<(n?n->name:"?")<<" position="<<pos.x<<","<<pos.y<<","<<pos.z<<std::endl;
    }
    for(size_t i=0;i<world.objects.GetCount();++i) {
        const auto bounds=world.aabb_objects[i];const auto c=bounds.getCenter(),h=bounds.getHalfWidth();
        std::cout<<"FIXTURE BOUNDS "<<i<<" center="<<c.x<<","<<c.y<<","<<c.z<<" half="<<h.x<<","<<h.y<<","<<h.z<<std::endl;
    }
    wi::Archive archive;world.Serialize(archive);
    if(!archive.SaveFile((project/"Content/Scenes/Main.wiscene").generic_u8string()))return false;
    std::ofstream(project/"magic-playable.json")<<nlohmann::json{{"projectId",projectId},{"assemblyId",assembly},{"abilityId",savedAbility.document.equipment.assetId},{"projectileId",savedProjectile.document.assetId},{"status","charged casting prototype; realistic fireball visual pending"}}.dump(2);
    std::cout<<"MAGIC NATIVE CHARGE/HOLD/RELEASE, SINGLE PSP RELEASE, NO AMMO, CANCEL AND SAVED PLAYER FIXTURE PASS\n";
    return true;
}

static bool MagicChannelPlayableProof(const fs::path& project,const fs::path& output) {
    using namespace renegade::runtime;
    nlohmann::json prior;std::ifstream(project/"magic-playable.json")>>prior;
    const std::string projectId=prior.at("projectId");std::string error;
    FirstPersonAssemblyService service;FirstPersonAssemblySettings settings;
    if(!service.ReadSettings(project.generic_u8string(),projectId,prior.at("assemblyId"),settings,error))return false;
    settings.spellHoldClip=9;
    for(auto& pair:settings.pairs){if(pair.action=="Charge")pair.armsClip=8;if(pair.action=="Release")pair.armsClip=10;}
    wi::scene::Scene native;
    if(!service.Prepare(project.generic_u8string(),projectId,settings,native,error)){std::cerr<<error<<std::endl;return false;}
    StableId assembly;
    if(!service.Save(project.generic_u8string(),projectId,"Channel Beam "+GenerateStableId().substr(0,8),settings,{},assembly,error))return false;
    RuntimePlayerViewRigState rig;
    for(size_t i=0;i<native.metadatas.GetCount();++i)if(native.metadatas[i].bool_values.has("renegade.first_person.spell"))
        rig.viewModelRoot=native.metadatas.GetEntity(i);
    RuntimePlayerViewAnimationState animation;
    if(!InitializeRuntimePlayerViewAnimations(native,rig,animation,error))return false;
    const auto startClip=animation.clips[PlayerViewActionIndex(PlayerViewAction::Charge)].front();
    const auto endClip=animation.clips[PlayerViewActionIndex(PlayerViewAction::Release)].front();
    const auto* startAnimation=native.animations.GetComponent(startClip.entity);
    const auto* endAnimation=native.animations.GetComponent(endClip.entity);
    if(!startAnimation || !endAnimation)return false;
    EquipmentDefinition ability{GenerateStableId(),"Continuous Energy Beam",assembly,EquipmentHandUse::PrimaryOnly,
        {{EquipmentAction::Cast,"Cast",0,0,0,
            endAnimation->end-endAnimation->start,false,true,true},
         {EquipmentAction::Equip,"Equip",0,0,.1f,0,false,true},
         {EquipmentAction::Unequip,"Unequip",0,0,.1f,0,false,true}}};
    EquipmentProjectileBinding binding;binding.action=EquipmentAction::Cast;
    binding.fireMode=EquipmentFireMode::Channel;binding.launchSocketName="Right Palm";
    binding.hitscanRangeMetres=25;binding.channelDamagePerSecond=20;ability.projectiles={binding};
    const auto saved=SaveEquipmentAsset(project.generic_u8string(),projectId,ability);
    if(!saved.succeeded){std::cerr<<saved.error<<std::endl;return false;}
    PlayerControllerSettings player;player.primaryEquipmentAssetId=saved.document.equipment.assetId;player.firstPersonArmsAssetId=assembly;
    RuntimeEquipmentLoadout loadout;if(!loadout.Load(project.generic_u8string(),projectId,player))return false;
    const auto tick=[&](GameplayInputFrame input,float dt) {
        const auto routed=loadout.RouteStaged(input,animation.equipped,animation.oneShotPlaying,dt,false,true);
        UpdateRuntimePlayerViewAnimations(native,animation,PlayerViewAction::Idle,dt,false,false,false,
            routed.toggleEquipmentPressed,true,loadout.chargePresentation,loadout.releasePresentation,false,0,0,routed.cancelEquipmentPressed);
        native.Update(dt);
    };
    GameplayInputFrame hold;hold.firePressed=hold.fireDown=true;tick(hold,.1f);
    if(!loadout.channelActive || animation.spellPhase!=1)return false;
    hold.firePressed=false;
    for(int i=0;i<20;++i)tick(hold,.1f);
    if(!loadout.channelActive || animation.spellPhase!=2 || animation.loadedShells!=0 ||
        !loadout.TakeProjectileRequests().empty())return false;
    if(!Capture(native,output/"beam-hold.png",true,true))return false;
    { // Measured beam-loop socket vs its actual animated palm knuckles.
        XMFLOAT3 beamSocket{},beamDirection{};std::string diagnosticError;
        if(!renegade::bridge::ReadLaunchSocketPose(native,rig.viewModelRoot,"Right Palm",beamSocket,beamDirection,diagnosticError))return false;
        std::cout<<"BEAM ORIGIN SOCKET "<<beamSocket.x<<","<<beamSocket.y<<","<<beamSocket.z<<"\\n";
        XMVECTOR knuckles=XMVectorZero();unsigned count=0;
        for(size_t i=0;i<native.names.GetCount();++i) {
            const auto& name=native.names[i].name;
            if(name=="index_01_r" || name=="middle_01_r" || name=="pinky_01_r") {
                const auto e=native.names.GetEntity(i);
                if(auto* t=native.transforms.GetComponent(e)){
                    auto pos=t->GetPosition();
                    std::cout<<"BEAM KNUCKLE "<<name<<" "<<pos.x<<","<<pos.y<<","<<pos.z<<"\\n";
                    knuckles+=XMLoadFloat3(&pos);++count;
                }
            }
        }
        if(count==3) {
            XMFLOAT3 center;XMStoreFloat3(&center,knuckles/3.f);
            std::cout<<"BEAM PALM CENTER "<<center.x<<","<<center.y<<","<<center.z<<"\\n";
            std::cout<<"BEAM SOCKET DELTA "<<beamSocket.x-center.x<<","<<beamSocket.y-center.y<<","<<beamSocket.z-center.z<<"\\n";
        }
    }
    // GPU-ready ownership/lifecycle proof for native visual meshes, light
    // and three bounded particle emitters; no desktop input or focus change.
    wi::scene::Scene visualScene;RuntimeChannelVisual visual;
    visual.Update(visualScene,{0,1,0},{0,1,6},true,1.f/60);
    // Beam geometry must begin at the palm and terminate at the hit, including
    // at long range. A prior .52 scale extended the full-length sphere 0.5m
    // behind the hand on a 25m ray, visibly projecting below the right palm.
    visual.Update(visualScene,{0,1,0},{0,1,25},true,1.f/60);
    const auto* core=visualScene.transforms.GetComponent(visual.core);
    if(!core || std::abs(core->scale_local.x-.021f)>.0001f ||
       std::abs(core->scale_local.z-12.5f)>.0001f)return false;
    const auto coreMatrix=core->GetWorldMatrix();
    XMFLOAT3 actualBegin{},actualEnd{};
    XMStoreFloat3(&actualBegin,XMVector3TransformCoord(XMVectorSet(0,0,-1,1),coreMatrix));
    XMStoreFloat3(&actualEnd,XMVector3TransformCoord(XMVectorSet(0,0,1,1),coreMatrix));
    if(std::abs(actualBegin.x)>.001f||std::abs(actualBegin.y-1.f)>.001f||
       std::abs(actualBegin.z)>.001f||std::abs(actualEnd.z-25.f)>.001f)return false;
    if(visual.root==wi::ecs::INVALID_ENTITY ||
       visualScene.emitters.GetCount()!=4+RuntimeChannelVisual::ShedEmitters ||
       visualScene.lights.GetCount()!=1 ||
       visualScene.objects.GetCount()!=3+RuntimeChannelVisual::Filaments*RuntimeChannelVisual::Segments+
           RuntimeChannelVisual::PulseCount*2)return false;
    const float firstPulse=RuntimeChannelVisual::PulseDistance(1.f/60,0,6.f);
    const float secondPulse=RuntimeChannelVisual::PulseDistance(.1f,0,6.f);
    if(!(firstPulse>=0 && secondPulse>firstPulse &&
         RuntimeChannelVisual::PulseDistance(.1f,RuntimeChannelVisual::PulseCount-1,6.f)<0))return false;
    const float firstSeconds=visual.seconds;
    visual.Update(visualScene,{0,1,0},{0,1,6},true,0);
    if(visual.seconds!=firstSeconds)return false;
    visual.Update(visualScene,{0,1,0},{0,1,4},false,.1f);
    if(visualScene.emitters.GetCount()!=4+RuntimeChannelVisual::ShedEmitters)return false;
    const auto* sparks=visualScene.emitters.GetComponent(visual.contactSparks);
    const auto* embers=visualScene.emitters.GetComponent(visual.impactEmbers);
    if(!sparks||!embers||sparks->gravity.y>=-1.f||embers->gravity.y>=-1.f)return false;
    visual.Reset(visualScene);
    if(visualScene.emitters.GetCount()!=0 || visualScene.lights.GetCount()!=0 ||
       visualScene.objects.GetCount()!=0)return false;
    // Native GPU-backed scorch proof: project soot on actual wall geometry and
    // verify it remains owned by the impact system after the beam is gone.
    wi::scene::Scene markScene;
    const auto receiver=markScene.Entity_CreateCube("Channel scorch test wall");
    if(auto* t=markScene.transforms.GetComponent(receiver)) {
        t->Scale(XMFLOAT3{40.f,2.f,.2f});t->Translate(XMFLOAT3{0.f,1.f,6.f});t->UpdateTransform();
    }
    markScene.Update(0);
    wi::vector<wi::scene::Scene::RayIntersectionResult> wallHits;
    markScene.IntersectsAll(wallHits,
        wi::primitive::Ray(XMFLOAT3{0,1,4},XMFLOAT3{0,0,1},0,4),
        wi::enums::FILTER_OPAQUE|wi::enums::FILTER_TRANSPARENT);
    if(wallHits.empty())return false;
    const auto& hit=wallHits.front();
    renegade::bridge::ProjectileImpact scorch;
    scorch.contact.surfaceType=renegade::bridge::ImpactSurfaceType::Concrete;
    scorch.contact.position={hit.position.x,hit.position.y,hit.position.z};
    scorch.contact.normal={hit.normal.x,hit.normal.y,hit.normal.z};
    scorch.damage=20.f;
    renegade::runtime::RuntimeProjectileVisuals marks;
    marks.PresentChannelScorch(markScene,scorch,receiver);
    if(!marks.impactDecals.empty() || marks.channelScorchCanvas.patches.size()!=1 ||
       markScene.decals.GetCount()!=1)return false;
    marks.PresentChannelScorch(markScene,scorch,receiver); // repaint uses same GPU surface
    if(marks.channelScorchCanvas.patches.size()!=1)return false;
    // Ten quick neighbouring stamps must not consume ten native decals.
    for(unsigned i=1;i<=10;++i) {
        scorch.contact.position.x=hit.position.x+float(i)*.035f;
        marks.PresentChannelScorch(markScene,scorch,receiver);
    }
    if(marks.channelScorchCanvas.patches.size()!=1 || markScene.decals.GetCount()!=1)return false;
    const auto firstPaintedPatch=marks.channelScorchCanvas.patches.front().decal;
    // Rendering uses Wickeds box_to_uv(local) = (.5*x+.5, -.5*y+.5).
    // V6 erroneously used (x+.5, .5-y), making every moving stamp
    // appear almost twice as far from the actual hit as it should.
    const auto* patchTransform=markScene.transforms.GetComponent(firstPaintedPatch);
    if(!patchTransform)return false;
    XMFLOAT3 offsetContact;
    XMStoreFloat3(&offsetContact,XMVector3TransformCoord(
        XMVectorSet(.32f,.24f,0,1),patchTransform->GetWorldMatrix()));
    scorch.contact.position=renegade::runtime::ProjectileBridgeVector(offsetContact);
    marks.PresentChannelScorch(markScene,scorch,receiver);
    if(marks.channelScorchCanvas.patches.size()!=1)return false;
    const auto actualUV=renegade::runtime::RuntimeChannelScorchCanvas::UV({.32f,.24f,0});
    if(std::abs(actualUV.x-.66f)>.0001f||std::abs(actualUV.y-.38f)>.0001f)return false;
    constexpr unsigned pixels=renegade::runtime::RuntimeChannelScorchCanvas::Resolution;
    const auto& painted=marks.channelScorchCanvas.patches.front().pixels;
    const auto alphaAt=[&](float u,float v) {
        const unsigned x=std::min(pixels-1,unsigned(u*pixels));
        const unsigned y=std::min(pixels-1,unsigned(v*pixels));
        return painted[(y*pixels+x)*4+3];
    };
    if(alphaAt(.66f,.38f)<100 || alphaAt(.82f,.26f)>0)return false;
    // Restore the original wall height after probing the off-axis spot.
    // The long stroke must sweep across the same physical wall row.
    scorch.contact.position.y=hit.position.y;
    scorch.contact.position.z=hit.position.z;
    // V5 hit FIFO eviction after ~20m (192 decals). Prove a 34m wall
    // stroke uses only area patches and keeps the very first painted tile.
    for(unsigned i=4;i<330;++i) {
        scorch.contact.position.x=hit.position.x+float(i)*.105f;
        marks.PresentChannelScorch(markScene,scorch,receiver);
    }
    std::cout<<"LONG SCORCH PATCHES="<<marks.channelScorchCanvas.patches.size()
        <<" FIRST="<<markScene.decals.Contains(firstPaintedPatch)
        <<" DABS="<<marks.channelScorchCanvas.paintedDabs<<"\\n";
    const auto patchCount=marks.channelScorchCanvas.patches.size();
    if(patchCount>36 || patchCount<5 ||
       !markScene.decals.Contains(firstPaintedPatch))return false;
    marks.channelScorchCanvas.Sync(markScene,.02f);
    std::cout<<"LONG SCORCH UPLOAD="<<
        markScene.materials.GetComponent(firstPaintedPatch)->textures[wi::scene::MaterialComponent::BASECOLORMAP].resource.IsValid()<<"\\n";
    const auto* soot=markScene.materials.GetComponent(firstPaintedPatch);
    if(!soot || !soot->textures[wi::scene::MaterialComponent::BASECOLORMAP].resource.IsValid())return false;
    // Visible registration targets: small coloured pins at the original and
    // displaced ray contact. These should sit at the centre of soot marks.
    const auto contactPin=[&](const char* name,const XMFLOAT3& pos,XMFLOAT4 color){
        const auto e=markScene.Entity_CreateSphere(name,.095f,12,12);
        auto* t=markScene.transforms.GetComponent(e);
        // Test wall is the plane z~5.9 seen by a camera at the origin;
        // put visual registration pins clearly in front of that plane.
        XMFLOAT3 p{pos.x,pos.y,pos.z-.18f};
        t->Translate(p);t->UpdateTransform();
        if(auto* m=markScene.materials.GetComponent(e)){
            m->baseColor=color;m->emissiveColor={color.x,color.y,color.z,4.f};m->SetDirty();
        }
    };
    const XMFLOAT3 originalContact=renegade::runtime::ProjectileNativeVector(
        renegade::bridge::ProjectileVector{hit.position.x,hit.position.y,hit.position.z});
    contactPin("Ground truth initial hit",originalContact,{.1f,1.f,.1f,1});
    contactPin("Ground truth offcenter hit",offsetContact,{1.f,.2f,.1f,1});
    markScene.Update(0);
    if(!Capture(markScene,output/"scorch-alignment-v7.png",true,true,true))return false;
    // The anchor and material survive the casting root's removal; only scene
    // reset or mark lifetime expiry should erase the painted wall.
    marks.channelScorchCanvas.Sync(markScene,8.f);
    if(markScene.decals.GetCount()!=patchCount)return false;
    marks.channelScorchCanvas.Sync(markScene,121.f);
    if(markScene.decals.GetCount()!=0)return false;
    marks.Reset(markScene);
    if(markScene.decals.GetCount()!=0)return false;
    const float time=animation.spellTime;tick(hold,0);
    if(animation.spellTime!=time || !loadout.channelActive)return false;
    tick({},.1f);if(loadout.channelActive || animation.spellPhase!=3)return false;
    if(!Capture(native,output/"beam-end.png",true,true))return false;
    for(int i=0;i<20;++i)tick({},.1f);
    hold.firePressed=true;tick(hold,.1f);hold.firePressed=false;
    for(int i=0;i<15;++i)tick(hold,.1f);
    hold.cancelEquipmentPressed=true;tick(hold,.1f);
    if(loadout.channelActive || animation.spellPhase!=0)return false;
    SceneService world;wi::Archive input((project/"Content/Scenes/Main.wiscene").generic_u8string(),true);
    if(!input.IsOpen())return false;world.GetScene().Serialize(input);
    auto start=ResolvePlayerStart(world.GetScene());if(start.resolution!=PlayerStartResolution::Success)return false;
    SetPlayerControllerSettingsCommand assign(world.GetScene(),start.start.entity,player);if(!assign.Execute())return false;
    for(const auto entity:CollectCharacters(world.GetScene()))SyncNativeCharacterPoseFromTransform(world.GetScene(),entity);
    world.GetScene().Update(0);
    wi::Archive archive;world.GetScene().Serialize(archive);
    if(!archive.SaveFile((project/"Content/Scenes/Main.wiscene").generic_u8string()))return false;
    std::ofstream(project/"channel-playable.json")<<nlohmann::json{{"projectId",projectId},{"assemblyId",assembly},
        {"abilityId",saved.document.equipment.assetId},{"range",25},{"damagePerSecond",20}}.dump(2);
    std::cout<<"CHANNEL NATIVE START/LOOP/END PAUSE RELEASE CANCEL NO PROJECTILE/AMMO AND SAVED FIXTURE PASS"<<std::endl;
    return true;
}
