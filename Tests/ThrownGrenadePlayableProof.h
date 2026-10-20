#pragma once
#include "../Runtime/src/RuntimeThrownHeldVisual.h"
// Private throw-playground builder. Source assets imported previously in P11.
// Never mutate a non-disposable owner scene or another project's registry.
static bool ThrownGrenadePlayableProof(const fs::path& project,const fs::path& output) {
    using namespace renegade::bridge;
    using namespace renegade::runtime;
    nlohmann::json j;
    {std::ifstream f(project/"thrown-grenade-reference.json");
        if(!f || !(f>>j))return false;}
    const auto projectId=j.at("projectId").get<std::string>();
    const auto armsId=j.at("armsId").get<std::string>();
    const auto grenadeId=j.at("grenadeId").get<std::string>();
    const auto projectileId=j.at("projectileId").get<std::string>();
    // Native import order differs from FBX source/visual display order.
    // Resolve explicitly from persisted native clip names; never use
    // assumed indices (the previous fixture used Walk as Throw).
    const auto& clipNames=j.at("clipNames");
    if(!clipNames.is_array() || clipNames.size()!=6)return false;
    const auto clipIndex=[&](const std::string& suffix)->unsigned {
        for(unsigned i=0;i<clipNames.size();++i)
            if(clipNames[i].is_string() &&
               clipNames[i].get<std::string>()=="Grenade_"+suffix)return i;
        return UINT_MAX;
    };
    const unsigned idle=clipIndex("Idle"),get=clipIndex("Get"),
        throwing=clipIndex("Throw"),walk=clipIndex("Walk"),
        run=clipIndex("Run"),holdClip=clipIndex("Hold");
    if(idle==UINT_MAX||get==UINT_MAX||throwing==UINT_MAX||
       walk==UINT_MAX||run==UINT_MAX||holdClip==UINT_MAX ||
       throwing!=j.at("throwClipIndex").get<unsigned>())return false;
    if(fs::exists(project/"thrown-grenade-playable.json") ||
       fs::exists(output/"throw-view-idle.png")){
        std::cerr<<"Thrown proof requires a fresh private project/output\n";return false;
    }
    fs::create_directories(output);
    std::string error;
    auto placed=CreatorAssetWorkflowService().PrepareModelPlacement(
         project.generic_u8string(),projectId,armsId);
    if(!placed.IsReady()){std::cerr<<placed.Result().error<<"\n";return false;}
    auto& source=*placed.PeekMutableScene();
    std::vector<PlayerViewBoneChoice> bones;
    if(!CollectPlayerViewBones(source,bones,error)) {
        std::cerr<<"Grenade hand bones: "<<error<<"\n";return false;
    }
    const auto choice=[&](const char* target)->const PlayerViewBoneChoice*{
        for(const auto& candidate:bones){
            auto path=nlohmann::json::parse(candidate.path,nullptr,false);
            if(path.is_array() && !path.empty() &&
               path.back().is_string() &&
               path.back().get<std::string>()==target)return &candidate;
        }
        return nullptr;
    };
    const auto palm=choice("Character001RArmPalm");
    if(!palm){std::cerr<<"Grenade palm bone missing\n";return false;}
    FirstPersonAssemblySettings settings;
    settings.armsAssetId=armsId;
    settings.spellPresentation=true;
    settings.spellHoldClip=holdClip;
    settings.parentBonePath=palm->path;
    // Based on FBX Camera001, with Blender Z-up converted to glTF Y-up.
    settings.cameraPosition={.05f,-1.72f,-.23f}; // Framing validated for both idle and grenade preparation.
    settings.cameraRotation={0,1,0,0}; // Correct authored camera faces the hands, confirmed native view proof.
    settings.weaponScale=1.f;
    settings.pairs={{"Idle",idle,0},{"Walk",walk,0},{"Run",run,0},
                    {"Charge",get,0},{"Release",throwing,0}};
    LaunchSocketDefinition psp;
    psp.name="Grenade Palm";psp.part=LaunchSocketPart::Arms;
    psp.parentPath=palm->path;
    psp.position={0,.012f,.10f};
    settings.launchSockets={psp};
    FirstPersonAssemblyService service;
    wi::scene::Scene view;
    if(!service.Prepare(project.generic_u8string(),projectId,settings,view,error)){
        std::cerr<<"THROW FPS ASSEMBLY "<<error<<"\n";return false;
    }
    // A real Throw must actually move the palm; this catches reversed
    // animation-index mappings even when the frames technically render.
    const auto palmPosition=[&](XMFLOAT3& result)->bool {
        for(size_t i=0;i<view.names.GetCount();++i) {
            if(view.names[i].name!="Character001RArmPalm")continue;
            const auto* tr=view.transforms.GetComponent(view.names.GetEntity(i));
            if(!tr)continue;
            result=tr->GetPosition();return true;
        }
        return false;
    };
    XMFLOAT3 earlyPalm{},latePalm{};
    if(!service.Pose(view,"Release",.20f,error) || !palmPosition(earlyPalm) ||
       !service.Pose(view,"Release",.64f,error) || !palmPosition(latePalm))
        return false;
    const float dx=latePalm.x-earlyPalm.x,dy=latePalm.y-earlyPalm.y,
        dz=latePalm.z-earlyPalm.z;
    if(dx*dx+dy*dy+dz*dz<.025f){
        std::cerr<<"THROWN GRENADE CLIP NOT ANIMATING: palm displacement too small\n";
        return false;
    }
    std::cout<<"GRENADE_NATIVE_THROW_PALM_DELTA "<<std::sqrt(dx*dx+dy*dy+dz*dz)<<"\n";
    for(const auto& sample:std::vector<std::pair<const char*,float>>{
          {"Idle",.15f},{"Charge",.45f},{"Release",.20f},
          {"Release",.64f},{"Release",1.05f}}){
        std::string filename="throw-view-";
        filename+=sample.first;filename+="-";
        filename+=std::to_string(int(sample.second*100))+".png";
        // At the forward release apex the throwing hand can exit the viewport:
        // keep the frame for review, but demand pixels at idle, wind-up and recovery.
        const bool offscreenAtRelease=std::string(sample.first)=="Release" &&
            sample.second>.50f && sample.second<1.50f;
        if(!service.Pose(view,sample.first,sample.second,error) ||
           !Capture(view,output/filename,true,true,offscreenAtRelease,.95f)){
            std::cerr<<"THROW FPS POSE "<<error<<"\n";return false;
        }
    }
    StableId assemblyId;
    if(!service.Save(project.generic_u8string(),projectId,
         "Hand Grenade - First Person Reference",settings,{},assemblyId,error)){
        std::cerr<<"THROW SAVE ASSEMBLY "<<error<<"\n";return false;
    }
    FirstPersonAssemblySettings reopened;
    if(!service.ReadSettings(project.generic_u8string(),projectId,assemblyId,reopened,error) ||
       reopened.launchSockets.size()!=1 || reopened.pairs.size()!=5 ||
       reopened.spellHoldClip!=holdClip || !reopened.spellPresentation)return false;
    EquipmentDefinition ability;
    if(!PrepareEquipmentFromAssembly(project.generic_u8string(),projectId,assemblyId,
         "Thrown Grenade P3 Playground",EquipmentHandUse::PrimaryOnly,ability,error)){
        std::cerr<<"THROW EQUIPMENT "<<error<<"\n";return false;
    }
    ability.projectiles={{EquipmentAction::Release,projectileId,"Grenade Palm",.64f}};
    const auto savedAbility=SaveEquipmentAsset(project.generic_u8string(),projectId,ability);
    if(!savedAbility.succeeded){
        std::cerr<<"THROW SAVE EQUIPMENT "<<savedAbility.error<<"\n";return false;
    }
    ProjectileAssetDocument loadedProjectile;
    if(!LoadProjectileAsset(project.generic_u8string(),projectId,
                           projectileId,loadedProjectile,error))return false;
    RuntimeProjectileVisuals flyingVisual;
    if(!flyingVisual.Prepare(project.generic_u8string(),"",projectId,loadedProjectile)){
        std::cerr<<"GRENADE FLYING VISUAL FAILED "<<flyingVisual.error<<"\n";
        return false;
    }
    // The P29 flying GLB had geometry and UVs but zero embedded image
    // resources. Catch that exact visual regression after native import.
    const auto preparedFlight=flyingVisual.templates.find(projectileId);
    if(preparedFlight==flyingVisual.templates.end())return false;
    bool hasNativeFlightAlbedo=false;
    for(size_t i=0;i<preparedFlight->second.scene->materials.GetCount();++i){
        const auto& material=preparedFlight->second.scene->materials[i];
        const auto& texture=material.textures[
            wi::scene::MaterialComponent::BASECOLORMAP].resource;
        if(texture.IsValid() && texture.GetTexture().IsValid())
            hasNativeFlightAlbedo=true;
    }
    if(!hasNativeFlightAlbedo){
        std::cerr<<"GRENADE FLIGHT MATERIAL LOST AFTER NATIVE IMPORT\n";
        return false;
    }
    std::cout<<"GRENADE_FLIGHT_TEXTURE_NATIVE_PASS\n";
    const auto priorObjects=view.objects.GetCount();
    if(!flyingVisual.Spawn(view,projectileId,987,true) ||
       view.objects.GetCount()<=priorObjects){
        std::cerr<<"GRENADE FLYING MODEL FAILED "<<flyingVisual.error<<"\n";
        return false;
    }
    flyingVisual.Reset(view);
    // Exercise real gameplay routing, rather than trusting a saved equipment file.
    // LMB prepares/holds; release must dispatch one (and only one) projectile
    // at the authored .64-second point in the native Throw clip.
    RuntimePlayerViewRigState rig;
    for(size_t i=0;i<view.metadatas.GetCount();++i)
        if(view.metadatas[i].bool_values.has("renegade.first_person.spell") &&
           view.metadatas[i].bool_values.get("renegade.first_person.spell"))
            rig.viewModelRoot=view.metadatas.GetEntity(i);
    if(SetHeldGrenadeBodyVisible(view,rig.viewModelRoot,false)!=1 ||
       SetHeldGrenadeBodyVisible(view,rig.viewModelRoot,true)!=1) {
        std::cerr<<"GRENADE HELD MESH VISIBILITY/ROOT TEST FAILED\n";return false;
    }
    XMFLOAT3 testSocket{},testForward{};
    view.Update(0);
    if(!ReadLaunchSocketPose(view,rig.viewModelRoot,"Grenade Palm",
                           testSocket,testForward,error)){
        std::cerr<<"THROW MISSING NATIVE PALM PSP "<<error<<"\n";return false;
    }
    if(!std::isfinite(testSocket.x)||!std::isfinite(testSocket.y)||
       !std::isfinite(testSocket.z)||!std::isfinite(testForward.z))return false;
    std::cout<<"THROW_NATIVE_PSP "<<testSocket.x<<","<<testSocket.y<<","
             <<testSocket.z<<" direction "<<testForward.x<<","
             <<testForward.y<<","<<testForward.z<<"\n";
    RuntimePlayerViewAnimationState anim;
    if(!InitializeRuntimePlayerViewAnimations(view,rig,anim,error)){
        std::cerr<<"THROW NATIVE ANIMATION "<<error<<"\n";return false;
    }
    RuntimeEquipmentLoadout loadout;
    PlayerControllerSettings loadSettings;
    loadSettings.primaryEquipmentAssetId=savedAbility.document.equipment.assetId;
    if(!loadout.Load(project.generic_u8string(),projectId,loadSettings)){
        std::cerr<<"THROW LOADOUT "<<loadout.error<<"\n";return false;
    }
    const auto tick=[&](GameplayInputFrame input,float dt) {
        auto routed=loadout.RouteStaged(input,anim.equipped,anim.oneShotPlaying,dt,false,true);
        UpdateRuntimePlayerViewAnimations(view,anim,PlayerViewAction::Idle,dt,
            routed.firePressed,routed.reloadPressed,false,
            routed.toggleEquipmentPressed,true,loadout.chargePresentation,
            loadout.releasePresentation,false,0,0,input.cancelEquipmentPressed);
        loadout.AfterNativePresentation(anim);
        view.Update(1.f/60);
        for(auto& request:loadout.TakeProjectileRequests())
            if(anim.spellReleaseAccepted)
                loadout.ScheduleProjectile(std::move(request),anim.activeClip);
        return loadout.ReleaseProjectiles(anim.activeClip,anim.pairedTime,!anim.equipped);
    };
    GameplayInputFrame click,quiet;click.firePressed=click.fireDown=true;
    // One click begins the throw; merely holding must not repeat it.
    unsigned throws=0;
    const auto step=[&](GameplayInputFrame input,float dt) {
        for(const auto& request:tick(input,dt)){
            ++throws;
            if(request.projectile.assetId!=projectileId ||
               request.launchSocketName!="Grenade Palm" ||
               !request.projectile.grenade.enabled ||
               std::abs(request.releaseSeconds-.64f)>.001f)
                return false;
        }
        return true;
    };
    if(!step(click,.1f) || anim.spellPhase!=3 ||
       anim.activeAction!=PlayerViewAction::Release)return false;
    click.firePressed=false;
    for(int i=0;i<12;++i)
        if(!step(click,.1f))return false;
    for(int i=0;i<12;++i)
        if(!step(quiet,.1f))return false;
    if(throws!=1 || anim.spellPhase!=0){
        std::cerr<<"GRENADE CLICK THROW COUNT "<<throws
                 <<" spellPhase "<<anim.spellPhase<<"\n";return false;
    }
    // Re-arm after the button is released and throw the next one.
    click.firePressed=true;
    if(!step(click,.1f))return false;
    click.firePressed=false;
    for(int i=0;i<20;++i)if(!step(quiet,.1f))return false;
    if(throws!=2){
        std::cerr<<"GRENADE NEXT CLICK FAILED "<<throws<<"\n";return false;
    }
    std::cout<<"GRENADE_CLICK_ONE_THROW_PER_PRESS "<<throws<<"\n";
    // Disposable native level; preserve every existing asset in the project.
    SceneService level;auto& world=level.GetScene();
    const auto floor=world.Entity_CreateCube("Grenade bounce floor");
    if(auto* t=world.transforms.GetComponent(floor)){
        t->Scale(XMFLOAT3{30.f,.12f,30.f});
        t->Translate(XMFLOAT3{0,-.15f,0});t->UpdateTransform();
    }
    if(auto* o=world.objects.GetComponent(floor))
        o->filterMask|=wi::enums::FILTER_NAVIGATION_MESH;
    // Visible geometry alone does not support the player/Jolt character.
    // Use a static collider, matching the working sword and magic playgrounds.
    auto& floorBody=world.rigidbodies.Create(floor);
    floorBody.mass=0.f;
    floorBody.shape=wi::scene::RigidBodyPhysicsComponent::CollisionShape::BOX;
    floorBody.box.halfextents={30.f,.12f,30.f};
    const auto block=world.Entity_CreateCube("Grenade cover blocker");
    if(auto* t=world.transforms.GetComponent(block)){
        t->Scale(XMFLOAT3{.6f,1.f,.6f});
        t->Translate(XMFLOAT3{-3,1,8});t->UpdateTransform();
    }
    auto& coverBody=world.rigidbodies.Create(block);
    coverBody.mass=0.f;
    coverBody.shape=wi::scene::RigidBodyPhysicsComponent::CollisionShape::BOX;
    coverBody.box.halfextents={.6f,1.f,.6f};
    if(world.rigidbodies.GetComponent(floor)==nullptr ||
       world.rigidbodies.GetComponent(floor)->mass!=0.f ||
       world.rigidbodies.GetComponent(block)==nullptr)return false;
    for(int i=0;i<2;++i) {
        const auto root=world.Entity_CreateTransform(i?"Grenade target right":"Grenade target centre");
        if(auto* t=world.transforms.GetComponent(root)) {
            t->Translate(XMFLOAT3{float(i)*2.5f,0,8});
            t->UpdateTransform();
        }
        CharacterAuthoringSettings charSettings;
        charSettings.autonomous=false;charSettings.factionId="Neutral";
        MakeCharacterCommand promote(world,root,charSettings);
        if(!promote.Execute())return false;
        const auto body=world.Entity_CreateCube("Grenade dummy body");
        if(auto* t=world.transforms.GetComponent(body)) {
            t->Scale(XMFLOAT3{.32f,.68f,.24f});
            t->Translate(XMFLOAT3{0,1,0});t->UpdateTransform();
        }
        world.Component_Attach(body,root,true);
    }
    world.Entity_CreateLight("Grenade daylight",{0,8,0},{1,1,1},2.8f,10.f,
        wi::scene::LightComponent::DIRECTIONAL);
    world.weathers.Create(wi::ecs::CreateEntity());
    CreatePlayerStartCommand start(world,{});
    if(!start.Execute())return false;
    PlayerControllerSettings player;
    player.primaryEquipmentAssetId=savedAbility.document.equipment.assetId;
    player.firstPersonArmsAssetId=assemblyId;
    SetPlayerControllerSettingsCommand set(world,start.CreatedEntity(),player);
    if(!set.Execute())return false;
    for(const auto e:CollectCharacters(world))SyncNativeCharacterPoseFromTransform(world,e);
    world.Update(0);
    wi::Archive sceneFile;world.Serialize(sceneFile);
    const auto scenePath=(project/"Content/Scenes/Main.wiscene").generic_u8string();
    if(!sceneFile.SaveFile(scenePath))return false;
    // Reopen from disk and prove the static collision bodies survived serialization.
    wi::Archive verifyArchive(scenePath,true,false);
    if(!verifyArchive.IsOpen())return false;
    wi::scene::Scene verified;
    verified.Serialize(verifyArchive);
    unsigned rigidFloor=0,rigidCover=0;
    for(size_t i=0;i<verified.names.GetCount();++i){
        const auto id=verified.names.GetEntity(i);
        const auto& name=verified.names[i].name;
        if(name!="Grenade bounce floor" && name!="Grenade cover blocker")continue;
        const auto* body=verified.rigidbodies.GetComponent(id);
        if(!body || body->mass!=0.f ||
           body->shape!=wi::scene::RigidBodyPhysicsComponent::CollisionShape::BOX) {
            std::cerr<<"GRENADE COLLISION WAS NOT SAVED: "<<name<<"\n";
            return false;
        }
        if(name=="Grenade bounce floor")++rigidFloor;
        else ++rigidCover;
    }
    if(rigidFloor!=1 || rigidCover!=1){
        std::cerr<<"GRENADE STATIC FLOOR/COVER NOT PRESENT IN REOPENED SCENE\n";
        return false;
    }
    std::ofstream(project/"thrown-grenade-playable.json")<<nlohmann::json{
        {"projectId",projectId},{"assemblyId",assemblyId},
        {"equipmentId",savedAbility.document.equipment.assetId},
        {"projectileId",projectileId},
        {"releaseSeconds",.64f},
        {"status","experimental throwable grenade: animation, physics and visual acceptance pending"}
    }.dump(2);
    std::cout<<"THROWN GRENADE PLAYER ASSEMBLY, 0.64s RELEASE, TARGETS AND SAVED LEVEL PASS\n";
    return true;
}

static bool ThrownGrenadeCameraDiagnostic(const fs::path& project,const fs::path& output) {
    using namespace renegade::bridge;
    nlohmann::json data;{
        std::ifstream file(project/"thrown-grenade-playable.json");
        if(!file || !(file>>data))return false;
    }
    const auto projectId=data.at("projectId").get<std::string>();
    FirstPersonAssemblyService assembly;FirstPersonAssemblySettings settings;
    std::string error;
    if(!assembly.ReadSettings(project.generic_u8string(),projectId,
       data.at("assemblyId").get<std::string>(),settings,error)){
        std::cerr<<error<<"\n";return false;
    }
    fs::create_directories(output);
    for(const auto& attempt:std::vector<std::pair<std::string,XMFLOAT3>>{
        {"base",{0.f,-1.679f,.085f}},
        {"closerA",{0.f,-1.75f,-.15f}},
        {"closerB",{0.f,-1.79f,-.25f}},
        {"closerC",{0.f,-1.87f,-.35f}},
        {"closerD",{0.f,-1.73f,-.45f}},
        {"offset",{.05f,-1.72f,-.23f}}}){
        auto variant=settings;
        variant.cameraPosition=attempt.second;
        variant.cameraRotation={0.f,1.f,0.f,0.f};
        wi::scene::Scene view;
        if(!assembly.Prepare(project.generic_u8string(),projectId,variant,view,error)){
            std::cerr<<error<<"\n";return false;
        }
        if(attempt.first=="base"){
            for(const float seconds:{.10f,.30f,.65f,1.05f}){
                if(!assembly.Pose(view,"Release",seconds,error))return false;
                for(size_t i=0;i<view.names.GetCount();++i) {
                    if(view.names[i].name!="Character001RArmPalm")continue;
                    const auto* tr=view.transforms.GetComponent(view.names.GetEntity(i));
                    if(!tr)continue;
                    const auto p=tr->GetPosition();
                    std::cout<<"GRENADE_NATIVE_RPALM Release "<<seconds<<" pos="
                        <<p.x<<","<<p.y<<","<<p.z<<"\n";
                }
            }
        }
        if(!assembly.Pose(view,"Charge",.45f,error))return false;
        std::cout<<"GREN_VIEW "<<attempt.first<<" obj="<<view.objects.GetCount()<<"\n";
        if(!Capture(view,output/(attempt.first+"-fps.png"),true,true,true,1.f))
            return false;
    }
    return true;
}
