// Exercises the same Runtime dispatch/native paired-clock path as the player.
static void CheckReloadStages(const fs::path& root,const StableId& project) {
    using namespace renegade::runtime;
    EquipmentDefinition item{GenerateStableId(),"Crossbow stage test","",EquipmentHandUse::TwoHanded,
        {{EquipmentAction::PrimaryUse,"Attack",0,0,.5f,0},
         {EquipmentAction::Reload,"Reload",0,0,0,0}}};
    item.actions[1].stages={{"Cock","Reload",.5f,false},{"Load Bolt","ReloadPartial",.75f,true}};
    item.actions[1].resumeStages=true;
    std::string error,text;EquipmentAssetDocument parsed;
    Check(SerializeEquipmentAsset({project,item},text,error),"staged serialize "+error);
    auto json=nlohmann::json::parse(text);
    Check(json["schema_version"]==4 && DeserializeEquipmentAsset(text,parsed,error) &&
        parsed.equipment.actions[1].stages==item.actions[1].stages && parsed.equipment.actions[1].resumeStages,"stage schema roundtrip");
    auto bad=json;bad["actions"][1]["stages"][0]["refill_ammo"]=true;
    Check(!DeserializeEquipmentAsset(bad.dump(),parsed,error),"cock permitted early refill");
    bad=json;bad["actions"][1]["stages"][1]["refill_ammo"]=1;
    Check(!DeserializeEquipmentAsset(bad.dump(),parsed,error),"numeric stage boolean");
    bad=json;bad["actions"][1]["stages"][1]["active_seconds"]=-1;
    Check(!DeserializeEquipmentAsset(bad.dump(),parsed,error),"negative stage duration");
    bad=json;bad["actions"][1]["stages"][1]["name"]="Cock";
    Check(!DeserializeEquipmentAsset(bad.dump(),parsed,error),"duplicate stage names");
    bad=json;bad["actions"][1]["stages"][1]["extra"]=true;
    Check(!DeserializeEquipmentAsset(bad.dump(),parsed,error),"unknown stage field");
    bad=json;bad["schema_version"]=1;
    Check(!DeserializeEquipmentAsset(bad.dump(),parsed,error),"legacy schema accepted stage fields");
    const auto saved=SaveEquipmentAsset(root.generic_u8string(),project,item);
    Check(saved.succeeded && LoadEquipmentAsset(root.generic_u8string(),project,saved.document.equipment.assetId,parsed,error) &&
        parsed.equipment.actions[1].stages==item.actions[1].stages,"staged immutable save/reopen "+error);
    RuntimeEquipmentLoadout runtime;PlayerControllerSettings settings;
    settings.primaryEquipmentAssetId=saved.document.equipment.assetId;
    Check(runtime.Load(root.generic_u8string(),project,settings),"staged Runtime load "+runtime.error);
    wi::scene::Scene scene;RuntimePlayerViewAnimationState native;
    native.initialized=native.pairedAssembly=true;native.firearm.capacity=1;native.loadedShells=0;
    for(auto action:{PlayerViewAction::Idle,PlayerViewAction::Attack,PlayerViewAction::Reload,PlayerViewAction::ReloadPartial}) {
        for(const char* track:{"arms","weapon"}) {
            const auto entity=wi::ecs::CreateEntity();auto& clip=scene.animations.Create(entity);
            clip.start=0;clip.end=2;clip.Pause();
            native.clips[PlayerViewActionIndex(action)].push_back({entity,track,track});native.ownedAssemblyClips.push_back(entity);
        }
    }
    const auto tick=[&](GameplayInputFrame input,float dt) {
        auto routed=runtime.RouteStaged(input,native.equipped,native.oneShotPlaying,dt,native.aiming,false,false,false,false,0,1,.75f,native.loadedShells<native.firearm.capacity);
        UpdateRuntimePlayerViewAnimations(scene,native,PlayerViewAction::Idle,dt,routed.firePressed,routed.reloadPressed,
            routed.aimDown,routed.toggleEquipmentPressed,true,false,false,false,0,0,false,&runtime.reloadStageRequest);
        if(dt>0)runtime.AfterNativePresentation(native);
    };
    GameplayInputFrame reload,quiet,cancel,fire;
    reload.reloadPressed=true;cancel.cancelEquipmentPressed=true;fire.firePressed=true;
    tick(reload,.25f);
    Check(native.reloadStagePlaying && native.activeAction==PlayerViewAction::Reload && native.loadedShells==0 && runtime.actions.ReservedHands()==3,"cock start and two-hand reservation");
    tick(cancel,0);Check(native.reloadStagePlaying && native.pairedTime==.25f,"paused cancel changed stage");
    tick(quiet,.25f);
    Check(!native.oneShotPlaying && native.loadedShells==0 && runtime.actions.CompletedStages(runtime.primary.equipment.assetId,EquipmentHand::Primary)==1,"cock completion refilled ammo or lost progress");
    // Cancel at the boundary, while the next-stage event is still queued.
    tick(cancel,.1f);
    Check(!native.reloadStagePlaying && runtime.actions.ReservedHands()==0 && !runtime.reloadStageRequest.dispatch,"queued load dispatched after cancellation");
    tick(reload,.25f);
    Check(native.activeAction==PlayerViewAction::ReloadPartial && native.loadedShells==0 && native.pairedTime==.25f,"resume repeated cock");
    tick(cancel,.1f);
    Check(!native.oneShotPlaying && native.loadedShells==0 && runtime.actions.CompletedStages(runtime.primary.equipment.assetId,EquipmentHand::Primary)==1,"mid-load cancel granted ammunition");
    tick(reload,.25f);tick(quiet,.5f);
    Check(native.loadedShells==1 && native.reloadStageCompleted && runtime.actions.CompletedStages(runtime.primary.equipment.assetId,EquipmentHand::Primary)==0,"final load did not refill exactly once");
    tick(quiet,.1f);Check(runtime.actions.ReservedHands()==0,"final recovery retained hands");
    tick(reload,.1f);Check(!native.oneShotPlaying && runtime.actions.ReservedHands()==0,"full crossbow admitted reload");
    tick(fire,.1f);Check(native.shotAccepted && native.loadedShells==0,"loaded bolt not consumed by native fire");
    tick(quiet,2);tick(quiet,.1f);tick(reload,.5f);
    Check(native.loadedShells==0 && runtime.actions.CompletedStages(runtime.primary.equipment.assetId,EquipmentHand::Primary)==1,"new shot bypassed cock");
    native.clips[PlayerViewActionIndex(PlayerViewAction::ReloadPartial)].clear();
    tick(quiet,.1f);
    Check(runtime.actions.ReservedHands()==0 && native.loadedShells==0 && !native.reloadStageAccepted,"missing load pair fell back or deadlocked");
    Check(runtime.Load(root.generic_u8string(),project,settings) && runtime.actions.CompletedStages(runtime.primary.equipment.assetId,EquipmentHand::Primary)==0,"loadout reset retained cock state");
    ResetRuntimePlayerViewAnimations(scene,native);
    Check(!native.initialized && !native.reloadStagePlaying && !native.reloadStageCompleted,"native reset retained stage pulse");

    // Another firearm uses three stages and reuses one presentation pair.
    auto rifle=item;rifle.name="Three-stage rifle";rifle.handUse=EquipmentHandUse::EitherHand;
    rifle.actions[1].stages={{"Open","Reload",.5f,false},{"Insert","Reload",.5f,false},{"Close","ReloadPartial",.5f,true}};
    EquipmentActionState state;
    Check(state.Begin(rifle,EquipmentAction::Reload,EquipmentHand::Primary) && state.Begin(rifle,EquipmentAction::Reload,EquipmentHand::OffHand),"reusable staged hand instances");
    state.Update(.01f,true);state.CompleteActive(rifle.assetId,EquipmentHand::Primary);
    Check(state.CompletedStages(rifle.assetId,EquipmentHand::Primary)==1 && state.CompletedStages(rifle.assetId,EquipmentHand::OffHand)==0,"stage progress leaked across owner hands");
    Check(state.Cancel(rifle.assetId,EquipmentHand::Primary) && state.ReservedHands()==2,"scoped staged cancellation");
    Check(state.Begin(rifle,EquipmentAction::Reload,EquipmentHand::Primary),"rifle resume");
    state.Update(.01f,true);state.CompleteActive(rifle.assetId,EquipmentHand::Primary);
    Check(state.Channels()[0].stageIndex==2 && state.ReservedHands()==3,"three-stage rifle skipped or released hands");
    state.CompleteActive(rifle.assetId,EquipmentHand::Primary);state.Update(.01f,true);
    Check(state.ReservedHands()==2,"rifle completion released other owner");
    state.Reset();Check(state.CompletedStages(rifle.assetId,EquipmentHand::Primary)==0,"stage reset");
}
