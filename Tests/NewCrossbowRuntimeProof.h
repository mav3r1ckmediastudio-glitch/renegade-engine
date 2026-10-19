#pragma once
// New FBX2013 crossbow -- isolated runtime proof, never old UE crossbow assets.
static bool NewCrossbowRuntimeProof(const fs::path& input,const fs::path& output) {
    using namespace renegade::runtime;
    nlohmann::json ids;{std::ifstream f(input/"new-crossbow-assets.json");if(!f || !(f>>ids))return false;}
    const std::string projectId=ids.at("projectId"),assembly=ids.at("assemblyId"),equipmentId=ids.at("equipmentId");
    std::string error; FirstPersonAssemblyService service;FirstPersonAssemblySettings settings;
    if(!service.ReadSettings(input.generic_u8string(),projectId,assembly,settings,error)) {
        std::cerr<<"CROSSBOW NEW RUNTIME read "<<error<<"\n";return false;
    }
    wi::scene::Scene scene;
    if(!service.Prepare(input.generic_u8string(),projectId,settings,scene,error)) {
        std::cerr<<"CROSSBOW NEW RUNTIME prepare "<<error<<"\n";return false;
    }
    RuntimePlayerViewRigState rig;
    for(size_t i=0;i<scene.metadatas.GetCount();++i)
        if(scene.metadatas[i].bool_values.has("renegade.first_person.assembly") &&
            scene.metadatas[i].bool_values.get("renegade.first_person.assembly"))
            rig.viewModelRoot=scene.metadatas.GetEntity(i);
    if(rig.viewModelRoot==wi::ecs::INVALID_ENTITY)return false;
    RuntimePlayerViewAnimationState native;
    if(!InitializeRuntimePlayerViewAnimations(scene,rig,native,error)){
        std::cerr<<"CROSSBOW NEW RUNTIME animation "<<error<<"\n";return false;
    }
    std::cout<<"NEW_CROSSBOW_RUNTIME_ANIMATION initialized="<<native.initialized
             <<" integrated="<<native.integratedAssembly
             <<" capacity="<<native.firearm.capacity<<"\n";
    if(!native.integratedAssembly||native.firearm.capacity!=1 || native.loadedShells!=1)return false;
    RuntimeEquipmentLoadout loadout;PlayerControllerSettings player;
    player.primaryEquipmentAssetId=equipmentId;
    if(!loadout.Load(input.generic_u8string(),projectId,player)) {
        std::cerr<<"CROSSBOW NEW RUNTIME loadout "<<loadout.error<<"\n";return false;
    }
    const auto tick=[&](GameplayInputFrame frame,float dt) {
        auto routed=loadout.RouteStaged(frame,native.equipped,native.oneShotPlaying,dt,
            native.aiming,false,false,false,false,0,1,.75f,native.loadedShells<native.firearm.capacity);
        UpdateRuntimePlayerViewAnimations(scene,native,PlayerViewAction::Idle,dt,
            routed.firePressed,routed.reloadPressed,routed.aimDown,routed.toggleEquipmentPressed,
            true,false,false,false,0,0,false,&loadout.reloadStageRequest);
        if(dt>0)loadout.AfterNativePresentation(native);
        scene.Update(1.f/60);
        for(auto& request:loadout.TakeProjectileRequests())
            if(native.shotAccepted)loadout.ScheduleProjectile(std::move(request),native.activeClip);
        return loadout.ReleaseProjectiles(native.activeClip,native.pairedTime,false);
    };
    GameplayInputFrame fire{},quiet{},reload{};
    fire.firePressed=true;reload.reloadPressed=true;
    auto release=tick(fire,.05f);
    std::cout<<"NEW_CROSSBOW_FIRE accepted="<<native.shotAccepted<<" ammo="<<native.loadedShells
        <<" immediate="<<release.size()<<"\n";
    if(!native.shotAccepted || native.loadedShells!=0)return false;
    for(unsigned i=0;i<8&&release.empty();++i)release=tick(quiet,.05f);
    if(release.size()!=1 || release[0].launchSocketName!="Crossbow PSP") {
        std::cerr<<"CROSSBOW NEW RUNTIME launch event missing or wrong socket\n";return false;
    }
    XMFLOAT3 muzzle{},direction{};
    if(!ReadLaunchSocketPose(scene,rig.viewModelRoot,release[0].launchSocketName,muzzle,direction,error)){
        std::cerr<<"CROSSBOW NEW RUNTIME socket "<<error<<"\n";return false;
    }
    std::cout<<"NEW_CROSSBOW_RUNTIME_PSP p="<<muzzle.x<<","<<muzzle.y<<","<<muzzle.z
        <<" d="<<direction.x<<","<<direction.y<<","<<direction.z<<"\n";
    const float distance=std::sqrt(muzzle.x*muzzle.x+muzzle.y*muzzle.y+muzzle.z*muzzle.z);
    if(distance>10 || direction.z<.2f) { std::cerr<<"NEW CROSSBOW PSP outside FPS range\n";return false;}
    RuntimeProjectileSession session;RuntimeProjectileVisuals visuals;
    const auto& projectile=release[0].projectile;
    if(!visuals.Prepare(input.generic_u8string(),"",projectId,projectile)){
        std::cerr<<visuals.error<<"\n";return false;
    }
    uint64_t boltId=0;ProjectileSource ownerInfo;ownerInfo.ownerSubjectId=GenerateStableId();
    ownerInfo.sourceAssetId=equipmentId;ownerInfo.factionId="Player";
    ownerInfo.knownPosition=ProjectileBridgeVector(muzzle);
    if(!session.Launch(projectile,ownerInfo,muzzle,direction,boltId) ||
        !visuals.Spawn(scene,projectile.assetId,boltId,true)){
        std::cerr<<session.lastError<<" "<<visuals.error<<"\n";return false;
    }
    visuals.Sync(scene,session.simulation);
    auto target=scene.Entity_CreateCube("New Crossbow target");
    auto& t=*scene.transforms.GetComponent(target);
    t.Scale(XMFLOAT3{2,2,.1f});t.Translate(XMFLOAT3{muzzle.x+direction.x*4,
        muzzle.y+direction.y*4,muzzle.z+direction.z*4});t.UpdateTransform();
    scene.Update(1.f/60);
    RuntimeCharacterSystemState characters;
    ProjectileOwnerBinding owner{ownerInfo.ownerSubjectId,rig.viewModelRoot,&visuals.roots};
    wi::ecs::Entity hitEntity=wi::ecs::INVALID_ENTITY;
    const auto query=[&](const ProjectileRecord& record,const ProjectileVector& from,const ProjectileVector& to){
        return QueryProjectileSceneSegment(scene,characters,owner,record,from,to,~0u,&hitEntity);
    };
    std::vector<ProjectileImpact> impacts;
    for(int i=0;i<120 && session.impacted==0;++i){
        if(!session.Update(1.f/120,query,impacts))return false;
        for(const auto& impact:impacts)visuals.Impact(scene,impact,hitEntity);
        visuals.Sync(scene,session.simulation,1.f/120);scene.Update(1.f/60);
    }
    std::cout<<"NEW_CROSSBOW_BOLT launched="<<session.launched
        <<" impacted="<<session.impacted<<" target="<<target<<" hit="<<hitEntity
        <<" retained="<<visuals.retained.size()<<"\n";
    if(session.launched!=1||session.impacted!=1||hitEntity!=target||visuals.retained.size()!=1)return false;
    visuals.Reset(scene);session.Reset();scene.Entity_Remove(target);
    // The native crossbow reload clip is a single full 2.366-second animation.
    tick(quiet,.6f);tick(reload,.1f);
    std::cout<<"NEW_CROSSBOW_RELOAD_START active="<<PlayerViewActionName(native.activeAction)<<" busy="<<native.oneShotPlaying<<" time="<<native.pairedTime<<" ammo="<<native.loadedShells<<" reserved="<<loadout.actions.ReservedHands()<<"\n";
    for(int i=0;i<35 && native.loadedShells==0;++i)tick(quiet,.1f);
    std::cout<<"NEW_CROSSBOW_RELOAD ammo="<<native.loadedShells<<"\n";
    if(native.loadedShells!=1)return false;
    fs::create_directories(output);
    std::cout<<"NEW_CROSSBOW_RUNTIME_FIRE_IMPACT_RELOAD_PASS\n";
    return true;
}
