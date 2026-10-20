#pragma once
// Private owner-supplied Crossbow asset set: never bundled into source control.
static bool NewCrossbowFullPackProof(const fs::path& source,const fs::path& output) {
    using namespace renegade::bridge;
    if(fs::exists(output/AssetRegistryDocumentName) ||
       !fs::exists(source/"Crossbow_Hands_FullAnimationBank.glb"))return false;
    fs::create_directories(output.parent_path());
    SceneService empty;
    wi::Archive archive;
    empty.GetScene().Serialize(archive);
    const auto templatePath=fs::absolute(output.parent_path()/
        (output.filename().generic_u8string()+"-template.wiscene"));
    if(!archive.SaveFile(templatePath.generic_u8string()))return false;
    ProjectService projects;
    if(!projects.CreateProject(fs::absolute(output.parent_path()).generic_u8string(),
        output.filename().generic_u8string(),templatePath.generic_u8string())){
        std::cerr<<"CROSSBOW NEW PROJECT "<<projects.LastError()<<"\n";return false;
    }
    const auto projectId=projects.CurrentProject().projectId;
    auto candidate=ModelImportCandidateService().PrepareModel(
        (source/"Crossbow_Hands_FullAnimationBank.glb").generic_u8string());
    if(!candidate.IsReady()){
        std::cerr<<"NEW CROSSBOW IMPORT "<<candidate.Error()<<"\n";return false;
    }
    const auto& native=*candidate.PeekScene();
    const auto evidence=candidate.Evidence();
    std::cout<<"NEW_CROSSBOW_NATIVE_OBJECTS "<<native.objects.GetCount()
             <<" RIGS "<<native.armatures.GetCount()
             <<" SKINNED "<<evidence.skinnedMeshes
             <<" BONES "<<evidence.armatureBones
             <<" ANIMATIONS "<<native.animations.GetCount()<<"\n";
    if(native.animations.GetCount()!=10 || evidence.armatureBones<40 ||
       evidence.skinnedMeshes<2)return false;
    const std::map<std::string,std::string> mapped{
        {"Crossbow_Idle","Idle"},{"Crossbow_Walk","Walk"},
        {"Crossbow_Run","Run"},{"Crossbow_Shot","Attack"},
        {"Crossbow_Reload","Reload"},
        {"Crossbow_Aim_Idle","Unassigned"},
        {"Crossbow_Aim_Shot","Unassigned"},
        {"Crossbow_Aim_Walk","Unassigned"},
        {"Crossbow_Get","Unassigned"},{"Crossbow_Hide","Unassigned"}};
    std::vector<std::string> actions,clipNames;
    for(size_t i=0;i<native.animations.GetCount();++i){
        const auto* n=native.names.GetComponent(native.animations.GetEntity(i));
        if(!n || !mapped.count(n->name))return false;
        clipNames.push_back(n->name);
        actions.push_back(mapped.at(n->name));
        const auto duration=native.animations[i].end-native.animations[i].start;
        std::cout<<"NEW_CROSSBOW_CLIP "<<i<<" "<<n->name<<" seconds="
                 <<duration<<" action="<<actions.back()<<"\n";
        if(!(duration>.1f && duration<5.f))return false;
    }
    ModelImportCommitRequest req;
    req.projectRoot=output.generic_u8string();
    req.projectId=projectId;
    req.assetName="Crossbow Full Animated Hands";
    req.characterAsset=true;req.playerRole="arms";
    req.destinationFolder="Content/Arms";
    req.animationActions=actions;
    const auto saved=ModelImportCommitService().CommitModel(req,candidate);
    if(!saved.succeeded){std::cerr<<saved.error<<"\n";return false;}
    auto reopened=CreatorAssetWorkflowService().PrepareModelPlacement(
        req.projectRoot,projectId,saved.assetId);
    if(!reopened.IsReady() || reopened.PeekScene()->animations.GetCount()!=10){
        std::cerr<<"NEW CROSSBOW cold reopen failed\n";return false;
    }
    const auto find=[&](const std::string& name)->unsigned {
        for(unsigned i=0;i<clipNames.size();++i)if(clipNames[i]==name)return i;
        return UINT_MAX;
    };
    std::string error;
    std::vector<PlayerViewBoneChoice> bones;
    if(!CollectPlayerViewBones(*reopened.PeekScene(),bones,error)){
        std::cerr<<"NEW CROSSBOW bones: "<<error<<"\n";return false;
    }
    // One combined rig/mesh keeps authored weapon string, trigger, arrow,
    // gloves and arms synchronized. No older UE crossbow model reused.
    FirstPersonAssemblySettings settings;
    settings.armsAssetId=saved.assetId;
    settings.integratedWeaponPresentation=true;
    settings.firearm.capacity=1;
    settings.cameraPosition={.05f,-1.72f,-.23f};
    settings.cameraRotation={0,1,0,0};
    settings.weaponScale=1.f;
    for(const auto& b:bones)if(b.path.find("Character001RArmPalm")!=std::string::npos){settings.parentBonePath=b.path;break;}
    if(settings.parentBonePath.empty())return false;
    settings.pairs={{"Idle",find("Crossbow_Idle"),0},
        {"Walk",find("Crossbow_Walk"),0},
        {"Run",find("Crossbow_Run"),0},
        {"Attack",find("Crossbow_Shot"),0},
        {"Reload",find("Crossbow_Reload"),0},
        {"AimIn",find("Crossbow_Aim_Idle"),0},
        {"AimAttack",find("Crossbow_Aim_Shot"),0},
        {"Equip",find("Crossbow_Get"),0},
        {"Unequip",find("Crossbow_Hide"),0}};
    wi::scene::Scene assembled;
    FirstPersonAssemblyService service;
    if(!service.Prepare(req.projectRoot,projectId,settings,assembled,error)){
        std::cerr<<"NEW CROSSBOW assemble "<<error<<"\n";return false;
    }
    fs::create_directories(output);
    assembled.Entity_CreateLight("Crossbow proof working light",{0,1,0.5f},{1,1,1},40.f,10.f,wi::scene::LightComponent::POINT);
    assembled.Update(1.f/60);
    for(const auto& pose:std::vector<std::pair<std::string,float>>{
        {"Idle",.3f},{"Attack",.12f},{"Reload",1.2f},
        {"AimIn",.3f},{"AimAttack",.12f},{"Equip",.3f}}){
        if(!service.Pose(assembled,pose.first,pose.second,error)){
            std::cerr<<"NEW CROSSBOW pose "<<pose.first<<" "<<error<<"\n";return false;
        }
        if(!Capture(assembled,output/("native-"+pose.first+".png"),true,true,true,.9f))
            return false;
        std::cout<<"NEW_CROSSBOW_POSE "<<pose.first<<" PASS\n";
    }
    // Derive the projectile socket from the actual animated bolt tip, not a guessed muzzle.
    if(!service.Pose(assembled,"Idle",0,error))return false;
    CrossbowTransforms(assembled);
    const auto arrowEntity=Find(assembled,"Crossbow_Arrow_Mesh");
    const auto* arrowObject=assembled.objects.GetComponent(arrowEntity);
    const auto* arrowMesh=arrowObject?assembled.meshes.GetComponent(arrowObject->meshID):nullptr;
    const auto* arrowRig=arrowMesh?assembled.armatures.GetComponent(arrowMesh->armatureID):nullptr;
    if(!arrowMesh){std::cerr<<"NEW CROSSBOW animated bolt geometry not found\n";return false;}
    XMFLOAT3 tip{};float mostForward=-std::numeric_limits<float>::max();
    for(size_t i=0;i<arrowMesh->vertex_positions.size();++i){
        const auto local=arrowMesh->IsSkinned()&&arrowRig?
            wi::scene::SkinVertex(*arrowMesh,*arrowRig,uint32_t(i)):
            XMLoadFloat3(&arrowMesh->vertex_positions[i]);
        XMFLOAT3 world;XMStoreFloat3(&world,XMVector3TransformCoord(local,
            XMLoadFloat4x4(&assembled.matrix_objects[assembled.objects.GetIndex(arrowEntity)])));
        if(world.z>mostForward){mostForward=world.z;tip=world;}
    }
    LaunchSocketDefinition psp;psp.part=LaunchSocketPart::Arms;psp.name="Crossbow PSP";
    for(const auto& choice:CollectLaunchSocketParents(assembled)){
        const auto path=nlohmann::json::parse(choice.path,nullptr,false);
        if(path.is_array() && !path.empty() && path.back()=="P_Arrow"){psp.parentPath=choice.path;break;}
    }
    if(psp.parentPath.empty()){std::cerr<<"NEW CROSSBOW arrow socket parent missing\n";return false;}
    const auto parentEntity=ResolveLaunchSocketParent(assembled,psp.parentPath);
    const auto* parent=assembled.transforms.GetComponent(parentEntity);
    if(!parent)return false;
    const auto inverse=XMMatrixInverse(nullptr,parent->GetWorldMatrix());
    tip.z+=.045f;
    psp.position={0,0,0};
    XMFLOAT3 direction;XMStoreFloat3(&direction,
        XMVector3Normalize(XMVector3TransformNormal(XMVectorSet(0,0,1,0),inverse)));
    psp.rotationDegrees={XMConvertToDegrees(-std::asin(std::clamp(direction.y,-1.f,1.f))),
        XMConvertToDegrees(std::atan2(direction.x,direction.z)),0};
    settings.launchSockets={psp};
    std::cout<<"NEW_CROSSBOW_PSP "<<tip.x<<","<<tip.y<<","<<tip.z<<"\n";
    StableId assemblyId;
    if(!service.Save(req.projectRoot,projectId,"Crossbow Full Animation Diagnostic",
         settings,{},assemblyId,error)){std::cerr<<error<<"\n";return false;}
    // Bind the bolt from this same Unity pack to Renegade's existing projectile pipeline.
    auto bolt=ModelImportCandidateService().PrepareStaticModel(
        (source.parent_path()/"Crossbow_Bolt_Source.glb").generic_u8string());
    if(!bolt.IsReady()){std::cerr<<"NEW CROSSBOW bolt import "<<bolt.Error()<<"\n";return false;}
    ModelImportCommitRequest boltRequest;boltRequest.projectRoot=req.projectRoot;
    boltRequest.projectId=projectId;boltRequest.assetName="Crossbow Bolt";
    auto boltSaved=ModelImportCommitService().CommitStaticModel(boltRequest,bolt);
    if(!boltSaved.succeeded){std::cerr<<"NEW CROSSBOW bolt save "<<boltSaved.error<<"\n";return false;}
    auto projectile=MakeProjectilePreset(ProjectilePreset::Bolt);
    projectile.name="Crossbow Bolt";projectile.meshAssetId=boltSaved.assetId;
    projectile.stickOnImpact=true;
    const auto projectileSaved=SaveProjectileAsset(req.projectRoot,projectId,projectile);
    if(!projectileSaved.succeeded){std::cerr<<"NEW CROSSBOW projectile "<<projectileSaved.error<<"\n";return false;}
    EquipmentDefinition equipment;
    if(!PrepareEquipmentFromAssembly(req.projectRoot,projectId,assemblyId,"Crossbow",EquipmentHandUse::TwoHanded,equipment,error)){
        std::cerr<<"NEW CROSSBOW equipment "<<error<<"\n";return false;
    }
    equipment.projectiles={{EquipmentAction::PrimaryUse,projectileSaved.document.assetId,psp.name,.12f}};
    const auto equipmentSaved=SaveEquipmentAsset(req.projectRoot,projectId,equipment);
    if(!equipmentSaved.succeeded){std::cerr<<"NEW CROSSBOW equipment save "<<equipmentSaved.error<<"\n";return false;}
    SceneService level;auto& world=level.GetScene();
    const auto floor=world.Entity_CreateCube("Crossbow range floor");
    auto& floorTransform=*world.transforms.GetComponent(floor);
    floorTransform.Scale(XMFLOAT3{20,.1f,20});floorTransform.Translate(XMFLOAT3{0,-.1f,0});
    floorTransform.UpdateTransform();
    const auto target=world.Entity_CreateCube("Crossbow target");
    auto& targetTransform=*world.transforms.GetComponent(target);
    targetTransform.Scale(XMFLOAT3{1,1.8f,.25f});targetTransform.Translate(XMFLOAT3{0,1.4f,10});
    targetTransform.UpdateTransform();
    CreatePlayerStartCommand playerStart(world,{});
    if(!playerStart.Execute()){std::cerr<<"NEW CROSSBOW player start failed\n";return false;}
    auto playerSettings=CapturePlayerControllerSettings(world,playerStart.CreatedEntity());
    playerSettings.firstPersonArmsAssetId=assemblyId;
    playerSettings.primaryEquipmentAssetId=equipmentSaved.document.equipment.assetId;
    SetPlayerControllerSettingsCommand assignment(world,playerStart.CreatedEntity(),playerSettings);
    if(!assignment.Execute()){std::cerr<<"NEW CROSSBOW player assignment failed\n";return false;}
    wi::Archive levelArchive;world.Serialize(levelArchive);
    const auto current=projects.CurrentProject();
    if(!levelArchive.SaveFile((fs::u8path(current.rootPath)/fs::u8path(current.startupScene)).generic_u8string())){
        std::cerr<<"NEW CROSSBOW startup level save failed\n";return false;
    }
    std::cout<<"NEW_CROSSBOW_LEVEL_EQUIPMENT_PROJECTILE_PASS\n";
    nlohmann::json doc{{"projectId",projectId},{"armsId",saved.assetId},
        {"assemblyId",assemblyId},{"equipmentId",equipmentSaved.document.equipment.assetId},{"projectileId",projectileSaved.document.assetId},{"boltId",boltSaved.assetId},{"clipNames",clipNames},
        {"actions",actions},{"status","new ten-clip full pack native/assembly proof; projectile and equipment linked; manual gameplay checks pending"}};
    std::ofstream(output/"new-crossbow-assets.json")<<doc.dump(2);
    std::cout<<"NEW_CROSSBOW_NATIVE_10_CLIPS_AND_ASSEMBLY_PASS\n";
    return true;
}
