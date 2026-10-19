#pragma once
// Owner-provided grenade source stays private. No asset bundled into source tree.
static bool ThrownGrenadeAssetProof(const fs::path& source,const fs::path& output) {
    using namespace renegade::bridge;
    if(fs::exists(output/AssetRegistryDocumentName) ||
       !fs::exists(source/"Grenade_Hands_AnimationBank.glb") ||
       !fs::exists(source/"Grenade_Physics.glb"))return false;
    SceneService scene;wi::Archive archive;scene.GetScene().Serialize(archive);
    const auto templateFile=fs::absolute(output.parent_path()/
        (output.filename().generic_u8string()+"-template.wiscene"));
    if(!archive.SaveFile(templateFile.generic_u8string()))return false;
    ProjectService projects;
    if(!projects.CreateProject(fs::absolute(output.parent_path()).generic_u8string(),
         output.filename().generic_u8string(),templateFile.generic_u8string())){
        std::cerr<<"THROWN PROJECT "<<projects.LastError()<<"\n";return false;
    }
    const auto projectId=projects.CurrentProject().projectId;
    auto arms=ModelImportCandidateService().PrepareModel(
        (source/"Grenade_Hands_AnimationBank.glb").generic_u8string());
    if(!arms.IsReady()){
        std::cerr<<"THROWN ARMS "<<arms.Error()<<"\n";return false;
    }
    std::vector<std::string> actions, clipNames;
    auto* imported=arms.PeekScene();
    if(imported->animations.GetCount()!=6 || arms.Evidence().armatureBones<40 ||
       arms.Evidence().skinnedMeshes<1){
        std::cerr<<"THROWN RIG/CLIP COUNT "<<imported->animations.GetCount()<<"\n";return false;
    }
    unsigned throwIndex=UINT_MAX;
    for(size_t i=0;i<imported->animations.GetCount();++i){
        const auto* n=imported->names.GetComponent(imported->animations.GetEntity(i));
        if(!n)return false;
        const auto& label=n->name;
        clipNames.push_back(label);
        const auto match=[&](const char* suffix){return label.find(suffix)!=std::string::npos;};
        std::string action="Unassigned";
        if(match("Idle"))action="Idle";
        else if(match("Get"))action="Unassigned";
        else if(match("Walk"))action="Walk";
        else if(match("Run"))action="Run";
        else if(match("Throw")){action="Attack";throwIndex=unsigned(i);}
        actions.push_back(action);
        std::cout<<"THROWN ANIM "<<i<<" "<<label<<" "<<action<<" "<<
            (imported->animations[i].end-imported->animations[i].start)<<"s\n";
    }
    if(throwIndex==UINT_MAX ||
       std::count(actions.begin(),actions.end(),"Idle")!=1 ||
       std::count(actions.begin(),actions.end(),"Attack")!=1)return false;
    ModelImportCommitRequest request;
    request.projectRoot=output.generic_u8string();request.projectId=projectId;
    request.assetName="Grenade Throwing Arms";request.characterAsset=true;
    request.playerRole="arms";request.destinationFolder="Content/Arms";
    request.animationActions=actions;
    const auto savedArms=ModelImportCommitService().CommitModel(request,arms);
    if(!savedArms.succeeded){
        std::cerr<<"THROWN ARMS SAVE "<<savedArms.error<<"\n";return false;
    }
    auto mesh=ModelImportCandidateService().PrepareModel(
        (source/"Grenade_Physics.glb").generic_u8string());
    if(!mesh.IsReady()){
        std::cerr<<"THROWN MESH "<<mesh.Error()<<"\n";return false;
    }
    ModelImportCommitRequest model;
    model.projectRoot=output.generic_u8string();model.projectId=projectId;
    model.assetName="Grenade Physical Mesh";model.destinationFolder="Content/Projectiles";
    const auto savedMesh=ModelImportCommitService().CommitModel(model,mesh);
    if(!savedMesh.succeeded){
        std::cerr<<"THROWN MESH SAVE "<<savedMesh.error<<"\n";return false;
    }
    auto projectile=MakeProjectilePreset(ProjectilePreset::Thrown);
    projectile.name="Grenade natural bounce and rolling prototype";
    projectile.meshAssetId=savedMesh.assetId;
    projectile.speedMetresPerSecond=15;
    projectile.gravityScale=1;
    projectile.damage=0; // contact never damages directly; fuse owns the blast.
    projectile.lifetimeSeconds=5.f;
    projectile.grenade.enabled=true;
    projectile.grenade.radius=.078f;
    projectile.grenade.fuseSeconds=3.f;
    projectile.grenade.restitution=.25f;
    projectile.grenade.surfaceFriction=.55f;
    projectile.grenade.blastRadius=3.5f;
    projectile.grenade.blastDamage=65.f;
    const auto savedProjectile=SaveProjectileAsset(output.generic_u8string(),projectId,projectile);
    if(!savedProjectile.succeeded){
        std::cerr<<"THROWN PROJECTILE SAVE "<<savedProjectile.error<<"\n";return false;
    }
    auto reopened=CreatorAssetWorkflowService().PrepareModelPlacement(
        output.generic_u8string(),projectId,savedArms.assetId);
    if(!reopened.IsReady()||reopened.PeekScene()->animations.GetCount()!=6){
        std::cerr<<"THROWN COLD REOPEN FAILED\n";return false;
    }
    auto& render=*reopened.PeekMutableScene();
    if(!MagicPoseCapture(render,0,.15f,output/"arms-idle.png"))return false;
    if(!MagicPoseCapture(render,throwIndex,1.f,output/"throw-early.png"))return false;
    if(!MagicPoseCapture(render,throwIndex,2.9f,output/"throw-middle.png"))return false;
    if(!MagicPoseCapture(render,throwIndex,5.1f,output/"throw-late.png"))return false;
    nlohmann::json result{{"projectId",projectId},{"armsId",savedArms.assetId},
        {"grenadeId",savedMesh.assetId},{"projectileId",savedProjectile.document.assetId},
        {"throwClipIndex",throwIndex},{"actions",actions},{"clipNames",clipNames},
        {"status","native rig and grenade physics asset with opt-in bounce/fuse/radius; playable FPS hand-rig integration outstanding"}};
    std::ofstream(output/"thrown-grenade-reference.json")<<result.dump(2);
    std::cout<<"THROWN GRENADE NATIVE RIG + 5 ACTIONS, MESH AND COLD PROJECT PASS\n";
    return true;
}
