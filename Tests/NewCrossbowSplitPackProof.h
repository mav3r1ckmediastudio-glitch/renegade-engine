#pragma once
// Private full animated Crossbow pack. Source never committed or redistributed.
static bool NewCrossbowSplitPackProof(const fs::path& source,const fs::path& output) {
    using namespace renegade::bridge;
    if(fs::exists(output/AssetRegistryDocumentName))return false;
    SceneService blank;wi::Archive archive;
    blank.GetScene().Serialize(archive);
    const auto templatePath=fs::absolute(output.parent_path()/
      (output.filename().generic_u8string()+"-template.wiscene"));
    if(!archive.SaveFile(templatePath.generic_u8string()))return false;
    ProjectService projects;
    if(!projects.CreateProject(fs::absolute(output.parent_path()).generic_u8string(),
         output.filename().generic_u8string(),templatePath.generic_u8string())){
        std::cerr<<"CROSSBOW project "<<projects.LastError()<<"\n";return false;
    }
    const auto projectId=projects.CurrentProject().projectId;
    std::string assetIds[2];
    std::map<std::string,unsigned> index[2];
    const std::map<std::string,std::string> mapActions{
       {"Crossbow_Idle","Idle"},{"Crossbow_Walk","Walk"},
       {"Crossbow_Run","Run"},{"Crossbow_Shot","Attack"},
       {"Crossbow_Reload","Reload"},
       {"Crossbow_Aim_Idle","Unassigned"},
       {"Crossbow_Aim_Shot","Unassigned"},
       {"Crossbow_Aim_Walk","Unassigned"},
       {"Crossbow_Get","Unassigned"},{"Crossbow_Hide","Unassigned"}};
    for(unsigned part=0;part<2;++part) {
        const std::string kind=part==0?"Arms":"Weapon";
        auto candidate=ModelImportCandidateService().PrepareModel(
            (source/("Crossbow_"+kind+"_10ClipSkin.glb")).generic_u8string());
        if(!candidate.IsReady()){
            std::cerr<<"NEW CROSSBOW "<<kind<<" IMPORT "<<candidate.Error()<<"\n";
            return false;
        }
        const auto& sourceScene=*candidate.PeekScene();
        const auto evidence=candidate.Evidence();
        if(sourceScene.animations.GetCount()!=10 ||
            sourceScene.objects.GetCount()<(part==0?2:4) ||
            evidence.armatureBones<(part==0?40:6)) {
            std::cerr<<"NEW CROSSBOW rig or clip count "<<kind<<" "
                <<sourceScene.animations.GetCount()<<" bones "<<evidence.armatureBones<<"\n";
            return false;
        }
        std::vector<std::string> actions;
        for(unsigned i=0;i<sourceScene.animations.GetCount();++i){
            const auto* name=sourceScene.names.GetComponent(sourceScene.animations.GetEntity(i));
            if(!name || !mapActions.count(name->name))return false;
            index[part][name->name]=i;
            actions.push_back(mapActions.at(name->name));
            std::cout<<"CROSSBOW_NEW_NATIVE "<<kind<<" CLIP "<<name->name
                     <<" index "<<i<<" time "<<sourceScene.animations[i].end-sourceScene.animations[i].start<<"\n";
        }
        ModelImportCommitRequest req;
        req.projectRoot=output.generic_u8string();
        req.projectId=projectId;
        req.assetName="Crossbow Full Animated "+kind;
        req.characterAsset=true;
        req.playerRole=part==0?"arms":"weapon";
        req.destinationFolder=part==0?"Content/Arms":"Content/Weapons";
        req.animationActions=actions;
        auto saved=ModelImportCommitService().CommitModel(req,candidate);
        if(!saved.succeeded){std::cerr<<"CROSSBOW COMMIT "<<kind<<" "<<saved.error<<"\n";return false;}
        assetIds[part]=saved.assetId;
        auto reopened=CreatorAssetWorkflowService().PrepareModelPlacement(
            req.projectRoot,projectId,saved.assetId);
        if(!reopened.IsReady() || reopened.PeekScene()->animations.GetCount()!=10){
            std::cerr<<"NEW CROSSBOW cold reopen "<<kind<<"\n";return false;
        }
        std::cout<<"CROSSBOW_NEW_REOPEN "<<kind<<" PASS\n";
    }
    auto arms=CreatorAssetWorkflowService().PrepareModelPlacement(output.generic_u8string(),
        projectId,assetIds[0]);
    std::string error;
    std::vector<PlayerViewBoneChoice> bones;
    if(!arms.IsReady() ||
       !CollectPlayerViewBones(*arms.PeekScene(),bones,error)) {
        std::cerr<<"NEW CROSSBOW bones: "<<error<<"\n";return false;
    }
    FirstPersonAssemblySettings settings;
    settings.armsAssetId=assetIds[0];settings.weaponAssetId=assetIds[1];
    for(const auto& bone:bones) {
        const auto path=nlohmann::json::parse(bone.path,nullptr,false);
        if(path.is_array() && !path.empty() && path.back()=="Character001RArmPalm") {
            settings.parentBonePath=bone.path;break;
        }
    }
    if(settings.parentBonePath.empty()){std::cerr<<"Crossbow right palm missing\n";return false;}
    // FBX Camera001 at (0,+0.0852,+1.6786) in Blender Z-up;
    // glTF/native forward +Z maps to (0,+1.6786,-0.0852).
    settings.cameraPosition={0.f,-1.6786f,.0852f};
    settings.cameraRotation={0,1,0,0};
    settings.weaponScale=1.f;
    settings.pairs={{"Idle",index[0].at("Crossbow_Idle"),index[1].at("Crossbow_Idle")},
       {"Walk",index[0].at("Crossbow_Walk"),index[1].at("Crossbow_Walk")},
       {"Run",index[0].at("Crossbow_Run"),index[1].at("Crossbow_Run")},
       {"Attack",index[0].at("Crossbow_Shot"),index[1].at("Crossbow_Shot")},
       {"Reload",index[0].at("Crossbow_Reload"),index[1].at("Crossbow_Reload")},
       {"AimAttack",index[0].at("Crossbow_Aim_Shot"),index[1].at("Crossbow_Aim_Shot")},
       {"Equip",index[0].at("Crossbow_Get"),index[1].at("Crossbow_Get")},
       {"Unequip",index[0].at("Crossbow_Hide"),index[1].at("Crossbow_Hide")}};
    FirstPersonAssemblyService service;
    wi::scene::Scene scene;
    if(!service.Prepare(output.generic_u8string(),projectId,settings,scene,error)){
        std::cerr<<"NEW CROSSBOW ASSEMBLY "<<error<<"\n";return false;
    }
    for(const auto& sample:std::vector<std::pair<std::string,float>>{
        {"Idle",.30f},{"Attack",.12f},{"Reload",1.2f},
        {"AimAttack",.12f},{"Equip",.3f}}) {
        if(!service.Pose(scene,sample.first,sample.second,error)){
            std::cerr<<"NEW CROSSBOW POSE "<<sample.first<<" "<<error<<"\n";return false;
        }
        fs::create_directories(output);
        if(!Capture(scene,output/("new-crossbow-"+sample.first+".png"),true,true,true,.9f) ||
           !Capture(scene,output/("new-crossbow-orbit-"+sample.first+".png")))
            return false;
        std::cout<<"CROSSBOW_NEW_RENDER "<<sample.first<<" PASS\n";
    }
    StableId assembly;
    if(!service.Save(output.generic_u8string(),projectId,
        "Full Crossbow First Person Assembly",settings,{},assembly,error)){
        std::cerr<<"CROSSBOW SAVE "<<error<<"\n";return false;
    }
    std::ofstream(output/"crossbow-new-assets.json")<<
        nlohmann::json{{"projectId",projectId},{"armsId",assetIds[0]},
            {"weaponId",assetIds[1]},{"assemblyId",assembly},
            {"status","Full 10-clip assets imported and assembled; projectile/equipment playable integration outstanding"}}
            .dump(2);
    std::cout<<"CROSSBOW_NEW_SPLIT_NATIVE_ASSEMBLY_PASS\n";
    return true;
}
