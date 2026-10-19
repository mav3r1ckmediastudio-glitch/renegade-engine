#include "../Runtime/src/RuntimeProjectileVisuals.h"
#include <map>
// Source Blueprint mesh-to-camera layout, converted from UE centimetres/axes.
static void ApplyCrossbowPackCamera(FirstPersonAssemblySettings& s) {
    const auto fbxToUE=XMMatrixScaling(1,-1,-1);
    const auto ueToView=XMMatrixSet(0,0,1,0,1,0,0,0,0,1,0,0,0,0,0,1);
    const auto authored=XMMatrixRotationX(XMConvertToRadians(-5.287787f))*
        XMMatrixRotationY(XMConvertToRadians(-1.921141f))*XMMatrixRotationZ(XMConvertToRadians(-19.911278f));
    XMVECTOR scale,rotation,position;
    XMMatrixDecompose(&scale,&rotation,&position,fbxToUE*authored*ueToView);
    XMStoreFloat4(&s.cameraRotation,rotation);
    s.cameraPosition={-0.04410996f,-1.55712738f,-0.00510596f};
}
static void CrossbowTransforms(const wi::scene::Scene& scene) {
    for(size_t i=0;i<scene.objects.GetCount();++i) {
        const auto* mesh=scene.meshes.GetComponent(scene.objects[i].meshID);if(!mesh)continue;
        const auto* armature=scene.armatures.GetComponent(mesh->armatureID);
        wi::primitive::AABB bounds;
        for(size_t v=0;v<mesh->vertex_positions.size();++v) {
            const auto local=mesh->IsSkinned()&&armature?wi::scene::SkinVertex(*mesh,*armature,uint32_t(v)):XMLoadFloat3(&mesh->vertex_positions[v]);
            bounds.AddPoint(XMVector3TransformCoord(local,XMLoadFloat4x4(&scene.matrix_objects[i])));
        }
        const auto p=bounds.getCenter();const auto ext=bounds.getHalfWidth();
        std::cout<<"SKIN object="<<i<<" center="<<p.x<<","<<p.y<<","<<p.z<<" half="<<ext.x<<","<<ext.y<<","<<ext.z<<std::endl;
        if(armature) {
            const auto* t=scene.transforms.GetComponent(mesh->armatureID);
            const auto* n=scene.names.GetComponent(mesh->armatureID);
            if(t){const auto q=t->GetPosition();std::cout<<"ARMATURE "<<(n?n->name:"")<<" p="<<q.x<<","<<q.y<<","<<q.z<<std::endl;}
        }
    }
    for(size_t i=0;i<scene.names.GetCount();++i) {
        const auto entity=scene.names.GetEntity(i);const auto* t=scene.transforms.GetComponent(entity);
        if(!t)continue;const auto& name=scene.names[i].name;
        if(name!="hand_r" && name!="ik_hand_gun" && name!="Handle" && scene.hierarchy.Contains(entity))continue;
        const auto pos=t->GetPosition();const auto sc=t->GetScale();
        std::cout<<"XFORM "<<name<<" p="<<pos.x<<","<<pos.y<<","<<pos.z<<" s="<<sc.x<<","<<sc.y<<","<<sc.z<<std::endl;
    }
}
static bool CrossbowReopenProof(const fs::path& input,const fs::path& output) {
    nlohmann::json j;std::ifstream(input/"crossbow-assets.json")>>j;
    const std::string projectId=j.at("projectId"),assembly=j.at("assemblyId");std::string error;
    FirstPersonAssemblyService service;FirstPersonAssemblySettings s;
    if(!service.ReadSettings(input.generic_u8string(),projectId,assembly,s,error))return false;


    wi::scene::Scene native;
    if(!service.Prepare(input.generic_u8string(),projectId,s,native,error))return false;
    for(const auto& sample : std::vector<std::pair<std::string,float>>{{"Idle",0},{"Attack",0.12f},{"Reload",0.5f}}) {
        if(!service.Pose(native,sample.first,sample.second,error))return false;
        if(!Capture(native,output/("reopen-"+sample.first+".png"),true,true))return false;
    }
    std::cout<<"CROSSBOW COLD REOPEN IDLE ATTACK RELOAD RENDER PASS\n";
    return true;
}
// Manual private-pack proof. No third-party assets or machine paths in source.
static bool CrossbowAssetProof(const fs::path& input, const fs::path& project)
{
    if(fs::exists(project/AssetRegistryDocumentName)) { std::cerr << "Use a fresh disposable output directory\n";return false; }
    std::string error;
    SceneService blank;
    const auto templatePath=fs::absolute(input/"crossbow-template.wiscene");
    wi::Archive starter;blank.GetScene().Serialize(starter);
    if(!starter.SaveFile(templatePath.generic_u8string()))return false;
    ProjectService projects;
    if(!projects.CreateProject(fs::absolute(project.parent_path()).generic_u8string(),project.filename().generic_u8string(),templatePath.generic_u8string())) {
        std::cerr<<projects.LastError()<<'\n';return false;
    }
    const auto metadata=projects.CurrentProject();const auto projectId=metadata.projectId;
    StableId ids[2];
    unsigned idleArm = 0, fireArm = 0, idleWeapon = 0, fireWeapon = 0, reloadWeapon = 0;
    unsigned runArm = 0, jumpStartArm = 0, jumpLoopArm = 0, jumpLandArm = 0;
    for (unsigned part = 0; part < 2; ++part) {
        using M = wi::scene::MaterialComponent;
        std::vector<ModelImportTextureRelink> relinks;
        auto relink = [&](unsigned slot, const char* path) {
            relinks.push_back({0, slot, (input/path).generic_u8string()});
        };
        if (part == 0) relink(M::NORMALMAP,"exported/Character/Textures/UE4_Mannequin__normals.tga");
        else {
            relink(M::BASECOLORMAP,"exported/Textures/T_LightCrossBow_BaseColor.tga");
            relink(M::NORMALMAP,"exported/Textures/T_LightCrossBow_Normal.tga");
        }
        auto candidate = ModelImportCandidateService().PrepareModel((input/(part == 0 ?
            "prepared/Arms_Base.fbx" : "prepared/Weapon_Base.fbx")).generic_u8string(),relinks);
        if (!candidate.IsReady()) { std::cerr << candidate.Error() << '\n'; return false; }
        const auto before = candidate.Evidence();
        auto* scene = candidate.PeekMutableScene();
        std::vector<std::string> actions(scene->animations.GetCount(),"Unassigned");
        std::cout << "CROSSBOW BASE part=" << part << " clips=" << actions.size()
                  << " objects=" << scene->objects.GetCount() << " rigs=" << scene->armatures.GetCount() << '\n';
        const std::vector<std::pair<const char*,const char*>> clips = part == 0 ?
            std::vector<std::pair<const char*,const char*>>{{"Idle","prepared/Arms_Idle.fbx"},
            {"Attack","prepared/Arms_Fire.fbx"},{"Run","prepared/Arms_Run.fbx"},
            {"JumpStart","prepared/Arms_JumpStart.fbx"},{"JumpLoop","prepared/Arms_JumpLoop.fbx"},
            {"JumpLand","prepared/Arms_JumpEnd.fbx"}} :
            std::vector<std::pair<const char*,const char*>>{{"Idle","prepared/Weapon_Idle.fbx"},{"Attack","prepared/Weapon_Fire.fbx"},{"Reload","prepared/Weapon_Reload.fbx"}};
        for (const auto& clip : clips) {
            const unsigned first = static_cast<unsigned>(candidate.PeekScene()->animations.GetCount());
            if (!ModelImportCandidateService().AppendExternalAnimations(candidate,(input/clip.second).generic_u8string(),error)) {
                std::cerr << "CROSSBOW APPEND " << clip.first << ": " << error << '\n'; return false;
            }
            scene = candidate.PeekMutableScene();
            if (scene->animations.GetCount() != first+1) { std::cerr << "Expected one source take\n"; return false; }
            const std::map<std::string,float> durations = part==0 ?
                std::map<std::string,float>{{"Idle",3.0333333f},{"Attack",0.26666668f},{"Run",0.8333333f},
                    {"JumpStart",0.73333335f},{"JumpLoop",1.16666663f},{"JumpLand",0.46666667f}} :
                std::map<std::string,float>{{"Idle",1.f},{"Attack",0.625f},{"Reload",1.f}};
            if(std::abs((scene->animations[first].end-scene->animations[first].start)-durations.at(clip.first))>0.001f) {
                std::cerr<<"Source clip duration changed\n";return false;
            }
            std::cout << "CROSSBOW CLIP " << part << " " << clip.first << " duration=" << scene->animations[first].end-scene->animations[first].start << std::endl;
            scene->names.Create(scene->animations.GetEntity(first)).name = std::string("Crossbow / ")+clip.first;
            actions.push_back(std::string(clip.first).find("Jump")==0 ? "Unassigned" : clip.first);
            if (part == 0 && std::string(clip.first)=="Run") runArm=first;
            if (part == 0 && std::string(clip.first)=="JumpStart") jumpStartArm=first;
            if (part == 0 && std::string(clip.first)=="JumpLoop") jumpLoopArm=first;
            if (part == 0 && std::string(clip.first)=="JumpLand") jumpLandArm=first;
            if (part == 1 && std::string(clip.first)=="Idle") idleWeapon=first;
            if (part == 0 && std::string(clip.first)=="Idle") idleArm=first;
            if (part == 0 && std::string(clip.first)=="Attack") fireArm=first;
            if (part == 1 && std::string(clip.first)=="Attack") fireWeapon=first;
            if (part == 1 && std::string(clip.first)=="Reload") reloadWeapon=first;
        }
        if (candidate.Evidence().skinIndexFingerprint != before.skinIndexFingerprint ||
            candidate.Evidence().inverseBindFingerprint != before.inverseBindFingerprint ||
            std::any_of(candidate.ExternalAnimations().begin(),candidate.ExternalAnimations().end(),
                [](const auto& clip){return !clip.matchingRig;})) {
            std::cerr << "Crossbow rig invariants failed\n"; return false;
        }
        ModelImportCommitRequest request; request.projectRoot=project.generic_u8string(); request.projectId=projectId;
        request.assetName=part==0?"Crossbow Arms":"Crossbow Weapon";request.characterAsset=true;
        request.playerRole=part==0?"arms":"weapon";request.animationActions=actions;
        request.destinationFolder=part==0?"Content/Arms":"Content/Weapons";
        auto saved=ModelImportCommitService().CommitModel(request,candidate);
        if (!saved.succeeded) { std::cerr << saved.error << '\n';return false; }
        ids[part]=saved.assetId;
        auto reopened=CreatorAssetWorkflowService().PrepareModelPlacement(request.projectRoot,projectId,saved.assetId);
        if(!reopened.IsReady() || reopened.PeekScene()->animations.GetCount()!=actions.size())return false;
        ReusableModelAssetDocument document;CreatorModelImportRecipe recipe;
        if(!ReadReusableModelAssetDocument((project/fs::u8path(saved.assetProjectRelativePath)).generic_u8string(),document,error) ||
            !ParseCreatorModelImportOptions(nlohmann::json::parse(document.manifest.settingsJson).at("options").dump(),recipe,error))return false;
        wi::scene::Scene rebuilt;ImportModel_FBX((project/fs::u8path(saved.sourceProjectRelativePath)).generic_u8string(),rebuilt);
        if(!ApplyCreatorModelImportRecipe(rebuilt,request.projectRoot,projectId,recipe,error) || rebuilt.animations.GetCount()!=actions.size()) {
            std::cerr << "CROSSBOW RETAINED REBUILD " << error << '\n';return false;
        }
        if(!Capture(*reopened.PeekMutableScene(),project/(part==0?"arms.png":"weapon.png")))return false;
        std::cout << "CROSSBOW RETAINED REBUILD part=" << part << " PASS\n";
    }
    auto bolt=ModelImportCandidateService().PrepareStaticModel((input/"prepared/Bolt.glb").generic_u8string());
    if(!bolt.IsReady()){std::cerr<<bolt.Error()<<'\n';return false;}
    ModelImportCommitRequest request;request.projectRoot=project.generic_u8string();request.projectId=projectId;
    request.assetName="Crossbow Bolt";
    auto saved=ModelImportCommitService().CommitStaticModel(request,bolt);
    if(!saved.succeeded){std::cerr<<saved.error<<'\n';return false;}
    auto arms=CreatorAssetWorkflowService().PrepareModelPlacement(request.projectRoot,projectId,ids[0]);
    std::vector<PlayerViewBoneChoice> bones;
    if(!arms.IsReady() || !CollectPlayerViewBones(*arms.PeekScene(),bones,error))return false;
    FirstPersonAssemblySettings settings;settings.armsAssetId=ids[0];settings.weaponAssetId=ids[1];
    for(const auto& bone:bones)if(nlohmann::json::parse(bone.path).back()=="middle_01_r"){settings.parentBonePath=bone.path;break;}
    if(settings.parentBonePath.empty()){std::cerr<<"No gun mount bone\n";return false;}
    // The instantiated source Blueprint mounts its Child Actor at GripPoint,
    // a socket on middle_01_r. Convert the authored socket from the Unreal
    // reference pose into the retained FBX bone frame; use the native mount.
    Pose(*arms.PeekMutableScene(),0,"Idle");
    const auto mount=std::find_if(bones.begin(),bones.end(),[&](const auto& b){return b.path==settings.parentBonePath;});
    const auto basis=XMMatrixScaling(1,-1,-1);
    const auto childActor=XMMatrixRotationX(XMConvertToRadians(3.472321f))*
        XMMatrixRotationY(XMConvertToRadians(-1.790268f))*XMMatrixRotationZ(XMConvertToRadians(2.416335f))*
        XMMatrixTranslation(0.02978025f,0.02798292f,0.04810812f);
    const auto socket=XMMatrixRotationX(XMConvertToRadians(-4.847672f))*
        XMMatrixRotationY(XMConvertToRadians(-15.f))*XMMatrixRotationZ(XMConvertToRadians(175.f))*
        XMMatrixTranslation(-0.02457152f,0.06927716f,0.00000142f);
    const auto sourceBone=XMMatrixRotationQuaternion(XMVectorSet(0.141120001f,-0.145990160f,0.809463f,0.550946159f))*
        XMMatrixTranslation(0.341294145f,0.184118866f,1.310870547f);
    const auto local=XMMatrixScaling(2,2,2)*basis*childActor*socket*sourceBone*basis*
        XMMatrixInverse(nullptr,arms.PeekScene()->transforms.GetComponent(mount->entity)->GetWorldMatrix());
    XMVECTOR scale,rotation,position;
    if(!XMMatrixDecompose(&scale,&rotation,&position,local))return false;
    XMStoreFloat3(&settings.weaponPosition,position);XMStoreFloat4(&settings.weaponRotation,rotation);
    settings.weaponScale=XMVectorGetX(scale);
    ApplyCrossbowPackCamera(settings);
    // Initial diagnostic mount; visual alignment and reload choreography remain open.
    settings.pairs={{"Idle",idleArm,idleWeapon},{"Attack",fireArm,fireWeapon},{"Reload",idleArm,reloadWeapon},
        {"Run",runArm,idleWeapon},{"JumpStart",jumpStartArm,idleWeapon},
        {"JumpLoop",jumpLoopArm,idleWeapon},{"JumpLand",jumpLandArm,idleWeapon},
        {"ReloadPartial",idleArm,idleWeapon},{"Walk",idleArm,idleWeapon}};
    settings.firearm.capacity=1;
    // Author the PSP from the native idle geometry, in the weapon root frame.
    wi::scene::Scene muzzleScene;
    FirstPersonAssemblyService muzzleService;
    if(!muzzleService.Prepare(request.projectRoot,projectId,settings,muzzleScene,error) || !muzzleService.Pose(muzzleScene,"Idle",0,error))return false;
    auto weaponRoot=Find(muzzleScene,"Handle");const auto grip=Find(muzzleScene,"middle_01_r");
    while(weaponRoot!=wi::ecs::INVALID_ENTITY) {
        const auto* hierarchy=muzzleScene.hierarchy.GetComponent(weaponRoot);
        if(!hierarchy || hierarchy->parentID==grip)break;
        weaponRoot=hierarchy->parentID;
    }
    const auto* rootTransform=muzzleScene.transforms.GetComponent(weaponRoot);
    if(!rootTransform || weaponRoot==grip || grip==wi::ecs::INVALID_ENTITY ||
        !muzzleScene.hierarchy.Contains(weaponRoot) || muzzleScene.hierarchy.GetComponent(weaponRoot)->parentID!=grip)return false;
    std::vector<XMFLOAT3> points;float maximumZ=-std::numeric_limits<float>::max();
    for(size_t i=0;i<muzzleScene.objects.GetCount();++i) {
        if(!muzzleScene.Entity_IsDescendant(muzzleScene.objects.GetEntity(i),weaponRoot))continue;
        const auto* mesh=muzzleScene.meshes.GetComponent(muzzleScene.objects[i].meshID);
        const auto* armature=mesh?muzzleScene.armatures.GetComponent(mesh->armatureID):nullptr;
        if(!mesh)continue;
        for(size_t v=0;v<mesh->vertex_positions.size();++v) {
            const auto local=mesh->IsSkinned()&&armature?wi::scene::SkinVertex(*mesh,*armature,uint32_t(v)):XMLoadFloat3(&mesh->vertex_positions[v]);
            XMFLOAT3 world;XMStoreFloat3(&world,XMVector3TransformCoord(local,XMLoadFloat4x4(&muzzleScene.matrix_objects[i])));
            points.push_back(world);maximumZ=std::max(maximumZ,world.z);
        }
    }
    if(points.empty())return false;
    XMFLOAT3 tip{};unsigned tipPoints=0;
    for(const auto& point:points)if(point.z>maximumZ-.01f){tip.x+=point.x;tip.y+=point.y;++tipPoints;}
    tip.x/=tipPoints;tip.y/=tipPoints;tip.z=maximumZ+.02f;
    const auto inverseWeapon=XMMatrixInverse(nullptr,rootTransform->GetWorldMatrix());
    LaunchSocketDefinition psp;psp.name="Crossbow PSP";
    XMStoreFloat3(&psp.position,XMVector3TransformCoord(XMLoadFloat3(&tip),inverseWeapon));
    XMFLOAT3 direction;XMStoreFloat3(&direction,XMVector3Normalize(XMVector3TransformNormal(XMVectorSet(0,0,1,0),inverseWeapon)));
    psp.rotationDegrees={XMConvertToDegrees(-std::asin(std::clamp(direction.y,-1.f,1.f))),XMConvertToDegrees(std::atan2(direction.x,direction.z)),0};
    settings.launchSockets={psp};
    FirstPersonAssemblyService service;StableId assembly;
    if(!service.Save(request.projectRoot,projectId,"Crossbow Diagnostic Assembly",settings,{},assembly,error)) {
        std::cerr<<error<<'\n';return false;
    }
    FirstPersonAssemblySettings read;
    if(!service.ReadSettings(request.projectRoot,projectId,assembly,read,error) || read.pairs.size()!=settings.pairs.size())return false;
    wi::scene::Scene native;
    if(!service.Prepare(request.projectRoot,projectId,read,native,error) || !service.Pose(native,"Attack",0.12f,error)) {
        std::cerr<<error<<'\n';return false;
    }
    CrossbowTransforms(native);
    if(!Capture(native,project/"assembly-diagnostic.png") ||
        !Capture(native,project/"assembly-first-person.png",true,true))return false;
    auto projectile=MakeProjectilePreset(ProjectilePreset::Bolt);projectile.name="Crossbow Bolt";
    projectile.meshAssetId=saved.assetId;projectile.visualRotationDegrees={0,-90,0};projectile.stickOnImpact=true;
    const auto savedProjectile=SaveProjectileAsset(request.projectRoot,projectId,projectile);
    if(!savedProjectile.succeeded){std::cerr<<savedProjectile.error<<'\n';return false;}
    ProjectileAssetDocument reopenedProjectile;
    if(!LoadProjectileAsset(request.projectRoot,projectId,savedProjectile.document.assetId,reopenedProjectile,error))return false;
    EquipmentDefinition equipment;
    if(!PrepareEquipmentFromAssembly(request.projectRoot,projectId,assembly,"Crossbow Stage Fixture",EquipmentHandUse::TwoHanded,equipment,error))return false;
    for(auto& action:equipment.actions)if(action.action==EquipmentAction::Reload) {
        action.activeSeconds=0;action.resumeStages=true;
        action.stages={{"Cock","Reload",1.f,false},{"Load Bolt","ReloadPartial",1.f,true}};
    }
    equipment.projectiles={{EquipmentAction::PrimaryUse,savedProjectile.document.assetId,psp.name,0.12f}};
    const auto savedEquipment=SaveEquipmentAsset(request.projectRoot,projectId,equipment);
    if(!savedEquipment.succeeded){std::cerr<<savedEquipment.error<<'\n';return false;}
    SceneService level;auto& world=level.GetScene();
    const auto floor=world.Entity_CreateCube("Crossbow test floor");
    auto& floorTransform=*world.transforms.GetComponent(floor);floorTransform.Scale(XMFLOAT3{20,.1f,20});floorTransform.Translate(XMFLOAT3{0,-.1f,0});floorTransform.UpdateTransform();
    CreatePlayerStartCommand start(world,{});if(!start.Execute())return false;
    auto playerSettings=CapturePlayerControllerSettings(world,start.CreatedEntity());
    playerSettings.firstPersonArmsAssetId=assembly;playerSettings.primaryEquipmentAssetId=savedEquipment.document.equipment.assetId;
    SetPlayerControllerSettingsCommand assign(world,start.CreatedEntity(),playerSettings);
    if(!assign.Execute())return false;
    wi::Archive levelArchive;world.Serialize(levelArchive);
    if(!levelArchive.SaveFile((fs::u8path(metadata.rootPath)/fs::u8path(metadata.startupScene)).generic_u8string()))return false;
    std::ofstream(project/"crossbow-assets.json") << nlohmann::json{{"projectId",projectId},{"armsId",ids[0]},
        {"weaponId",ids[1]},{"boltId",saved.assetId},{"projectileId",savedProjectile.document.assetId},
        {"equipmentId",savedEquipment.document.equipment.assetId},{"assemblyId",assembly},
        {"status","staged runtime fixture; Load Bolt uses Idle arms; sound and owner gameplay acceptance pending"}}.dump(2);
    std::cout<<"CROSSBOW GOVERNED ASSETS AND ASSEMBLY REOPEN PASS\n";
    return true;
}

static bool CrossbowStagedRuntimeProof(const fs::path& input,const fs::path& output) {
    using namespace renegade::runtime;
    nlohmann::json ids;std::ifstream(input/"crossbow-assets.json")>>ids;
    const std::string projectId=ids.at("projectId"),assembly=ids.at("assemblyId");std::string error;
    FirstPersonAssemblyService service;FirstPersonAssemblySettings settings;
    if(!service.ReadSettings(input.generic_u8string(),projectId,assembly,settings,error))return false;
    wi::scene::Scene scene;
    if(!service.Prepare(input.generic_u8string(),projectId,settings,scene,error))return false;
    RuntimePlayerViewRigState rig;
    for(size_t i=0;i<scene.metadatas.GetCount();++i)if(scene.metadatas[i].bool_values.has("renegade.first_person.assembly") &&
        scene.metadatas[i].bool_values.get("renegade.first_person.assembly"))rig.viewModelRoot=scene.metadatas.GetEntity(i);
    RuntimePlayerViewAnimationState native;
    if(!InitializeRuntimePlayerViewAnimations(scene,rig,native,error))return false;
    RuntimeEquipmentLoadout loadout;PlayerControllerSettings player;
    player.primaryEquipmentAssetId=ids.at("equipmentId").get<std::string>();
    if(!loadout.Load(input.generic_u8string(),projectId,player))return false;
    const auto tick=[&](GameplayInputFrame input,float dt) {
        auto routed=loadout.RouteStaged(input,native.equipped,native.oneShotPlaying,dt,native.aiming,false,false,false,false,0,1,.75f,native.loadedShells<native.firearm.capacity);
        UpdateRuntimePlayerViewAnimations(scene,native,PlayerViewAction::Idle,dt,routed.firePressed,routed.reloadPressed,routed.aimDown,
            routed.toggleEquipmentPressed,true,false,false,false,0,0,false,&loadout.reloadStageRequest);
        if(dt>0)loadout.AfterNativePresentation(native);
        scene.Update(1.f/60);
        for(auto& request:loadout.TakeProjectileRequests())if(native.shotAccepted)loadout.ScheduleProjectile(std::move(request),native.activeClip);
        return loadout.ReleaseProjectiles(native.activeClip,native.pairedTime,false);
    };
    GameplayInputFrame quiet,fire,reload,cancel;fire.firePressed=true;reload.reloadPressed=true;cancel.cancelEquipmentPressed=true;
    if(!tick(fire,.05f).empty() || !native.shotAccepted || native.loadedShells!=0)return false;
    auto released=tick(quiet,.1f);
    if(released.size()!=1 || released[0].launchSocketName!="Crossbow PSP")return false;
    XMFLOAT3 muzzle,direction;
    if(!ReadLaunchSocketPose(scene,rig.viewModelRoot,released[0].launchSocketName,muzzle,direction,error))return false;

    CrossbowTransforms(scene);
    if(!Capture(scene,output/"runtime-fire-diagnostic.png",true,true))return false;
    wi::scene::Scene comparison;
    if(!service.Prepare(input.generic_u8string(),projectId,settings,comparison,error) || !service.Pose(comparison,"Attack",native.pairedTime,error))return false;
    wi::ecs::Entity comparisonRoot=wi::ecs::INVALID_ENTITY;
    for(size_t i=0;i<comparison.metadatas.GetCount();++i)if(comparison.metadatas[i].bool_values.has("renegade.first_person.assembly"))comparisonRoot=comparison.metadatas.GetEntity(i);
    XMFLOAT3 cp,cd;
    if(!ReadLaunchSocketPose(comparison,comparisonRoot,"Crossbow PSP",cp,cd,error))return false;
    std::cout<<"PREVIEW PSP p="<<cp.x<<","<<cp.y<<","<<cp.z<<" d="<<cd.x<<","<<cd.y<<","<<cd.z<<std::endl;
    std::cout<<"CROSSBOW PSP p="<<muzzle.x<<","<<muzzle.y<<","<<muzzle.z<<" d="<<direction.x<<","<<direction.y<<","<<direction.z<<std::endl;
    if(muzzle.z<=0 || direction.z<.5f){std::cerr<<"PSP faces behind the camera\n";return false;}
    std::cout<<"PSP CHECK PASS\n";
    RuntimeProjectileSession session;RuntimeProjectileVisuals visuals;
    const auto& projectile=released[0].projectile;
    if(!visuals.Prepare(input.generic_u8string(),"",projectId,projectile)){std::cerr<<visuals.error<<'\n';return false;}
    uint64_t bolt=0;ProjectileSource source;source.ownerSubjectId=GenerateStableId();source.sourceAssetId=player.primaryEquipmentAssetId;source.factionId="Player";source.knownPosition=ProjectileBridgeVector(muzzle);
    if(!session.Launch(projectile,source,muzzle,direction,bolt) || !visuals.Spawn(scene,projectile.assetId,bolt,true)){std::cerr<<session.lastError<<" "<<visuals.error<<std::endl;return false;}
    std::cout<<"BOLT APPEARANCE SPAWN PASS\n";
    visuals.Sync(scene,session.simulation);scene.Update(1.f/60);
    const auto target=scene.Entity_CreateCube("Bolt target");auto& targetTransform=*scene.transforms.GetComponent(target);
    targetTransform.Scale(XMFLOAT3{2,2,.1f});targetTransform.Translate(XMFLOAT3{muzzle.x+direction.x*4,muzzle.y+direction.y*4,muzzle.z+direction.z*4});targetTransform.UpdateTransform();
    scene.Update(1.f/60);RuntimeCharacterSystemState characters;
    ProjectileOwnerBinding owner{source.ownerSubjectId,rig.viewModelRoot,&visuals.roots};
    wi::ecs::Entity hitEntity=wi::ecs::INVALID_ENTITY;
    const auto query=[&](const ProjectileRecord& record,const ProjectileVector& from,const ProjectileVector& to){
        return QueryProjectileSceneSegment(scene,characters,owner,record,from,to,~0u,&hitEntity);
    };
    std::vector<ProjectileImpact> impacts;
    for(int i=0;i<120 && session.impacted==0;++i) {
        if(!session.Update(1.f/120,query,impacts))return false;
        for(const auto& impact:impacts)visuals.Impact(scene,impact,hitEntity);
        visuals.Sync(scene,session.simulation,1.f/120);scene.Update(1.f/60);
    }
    std::cout<<"BOLT WORLD launched="<<session.launched<<" impacted="<<session.impacted<<" hit="<<hitEntity<<" target="<<target<<" retained="<<visuals.retained.size()<<" remaining="<<session.simulation.Records().size()<<" error="<<session.lastError<<std::endl;
    if(session.launched!=1 || session.impacted!=1 || hitEntity!=target || visuals.retained.size()!=1)return false;
    if(!Capture(scene,output/"bolt-target-impact.png"))return false;
    visuals.Reset(scene);session.Reset();
    if(!session.simulation.Records().empty() || !visuals.retained.empty())return false;
    scene.Entity_Remove(target);
    tick(quiet,1);tick(quiet,.01f);
    tick(reload,.5f);if(native.loadedShells!=0 || !native.reloadStagePlaying)return false;
    if(!Capture(scene,output/"cock-half.png",true,true))return false;
    tick(quiet,.5f);
    if(native.loadedShells!=0 || loadout.actions.CompletedStages(player.primaryEquipmentAssetId,EquipmentHand::Primary)!=1)return false;
    tick(cancel,.01f);if(loadout.actions.ReservedHands()!=0 || native.reloadStagePlaying)return false;
    tick(reload,.5f);
    if(native.activeAction!=PlayerViewAction::ReloadPartial || native.loadedShells!=0)return false;
    if(!Capture(scene,output/"load-placeholder.png",true,true))return false;
    tick(cancel,.01f);if(native.loadedShells!=0 || native.oneShotPlaying)return false;
    tick(reload,1);tick(quiet,.01f);
    if(native.loadedShells!=1 || loadout.actions.ReservedHands()!=0)return false;
    if(!Capture(scene,output/"ready.png",true,true))return false;
    auto boltModel=CreatorAssetWorkflowService().PrepareModelPlacement(input.generic_u8string(),projectId,projectile.meshAssetId);
    if(!boltModel.IsReady())return false;
    if(!Capture(*boltModel.PeekMutableScene(),output/"bolt-appearance.png"))return false;
    std::cout<<"CROSSBOW REAL RIG STAGES, CANCELLATION/RESUME, PSP RELEASE, BOLT WORLD CONTACT/STICK/RESET PASS\n";
    return true;
}
