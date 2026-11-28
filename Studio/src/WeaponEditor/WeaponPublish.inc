// Weapon prefab publication builds a private preview and stages all project writes.
namespace {
using namespace renegade::bridge;
struct PublishScratch {
    fs::path path=fs::temp_directory_path()/fs::u8path("Renegade-weapon-"+GenerateStableId());
    PublishScratch(){fs::create_directories(path);}
    ~PublishScratch(){std::error_code ec;fs::remove_all(path,ec);}
};
void requirePublish(bool ok,const std::string& error){if(!ok)throw std::runtime_error(error);}
std::vector<uint8_t> prefabBytes(const fs::path& p){
    std::ifstream in(p,std::ios::binary);requirePublish(bool(in),"Missing prefab dependency: "+p.u8string());
    return {std::istreambuf_iterator<char>(in),{}};
}
void writePrefabBytes(const fs::path& p,const std::vector<uint8_t>& bytes){
    fs::create_directories(p.parent_path());std::ofstream file(p,std::ios::binary);
    file.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());file.close();
    requirePublish(bool(file),"Could not write "+p.u8string());
}
void saveNativePart(Scene& scene,const fs::path& p){
    struct Restore{wi::resourcemanager::Mode mode=wi::resourcemanager::GetMode();~Restore(){wi::resourcemanager::SetMode(mode);}} restore;
    wi::resourcemanager::SetMode(wi::resourcemanager::Mode::EMBED_FILE_DATA);
    wi::Archive archive(p.u8string(),false,false);archive.SetCompressionEnabled(true);
    scene.Serialize(archive);requirePublish(archive.SaveFile(p.u8string()),"Cannot serialize prefab model.");
}
void exportWeaponPresentation(const fs::path& destination, Scene& scene){
    // Preserve source images even when the publisher runs before the first rendered frame.
    // Only modify the detached publication scene. Keep each live editor resource intact.
    for(size_t i=0;i<scene.materials.GetCount();++i){
        for(auto& slot:scene.materials[i].textures){
            if(!slot.resource.IsValid() || slot.name.empty())continue;
            auto format=wi::helper::toUpper(wi::helper::GetExtensionFromFileName(slot.name));
            if(format!="PNG" && format!="JPG" && format!="JPEG" && format!="TGA")continue;
            const auto& retained=slot.resource.GetFileData();
            std::vector<uint8_t> bytes(retained.begin(),retained.end());
            if(bytes.empty() && fs::is_regular_file(fs::u8path(slot.name)))bytes=prefabBytes(fs::u8path(slot.name));
            requirePublish(!bytes.empty(),"Missing source image while publishing: "+slot.name);
            if(format=="TGA"){
                std::vector<uint8_t> png;
                requirePublish(EncodeMaterialTgaAsPng(bytes.data(),bytes.size(),png),"Could not encode "+slot.name);
                bytes=std::move(png);format="PNG";
            }
            auto name=(destination.parent_path()/fs::u8path(GenerateStableId()+(format=="PNG"?".png":".jpg"))).u8string();
            auto portable=wi::resourcemanager::Load(name,wi::resourcemanager::Flags::IMPORT_RETAIN_FILEDATA,bytes.data(),bytes.size());
            requirePublish(portable.IsValid() && portable.GetFileData().size()==bytes.size(),"Could not retain "+slot.name);
            slot.name=name;slot.resource=std::move(portable);
        }
    }
    ExportModel_GLTF(destination.u8string(),scene);
}
std::string prefabAction(const std::string& name){
    static const std::map<std::string,std::string> aliases={{"Fire","Attack"},{"ADS Fire","AimAttack"},{"ADS Idle","AimIn"},{"Sprint","Run"},{"Raise","Equip"},{"Lower","Unequip"}};
    auto it=aliases.find(name);if(it!=aliases.end())return it->second;
    for(auto action:FirstPersonAssemblyActions)if(name==action)return action;
    return {};
}
}
// Sample into the original parent space even while the reload changes magazine parent.
void Preview::bakePrefab(Scene& baked,json& recipe){
    requirePublish(assembled||(instance&&hand),"Fit the weapon to an arms rig before publishing.");
    attachMagazine(false);clearFlashPlayback();clearFlyingCasings();
    arms.Entity_Remove(arms.Entity_FindByName("Studio softbox"));arms.weathers.Clear();
    std::vector<Entity> nodes,parents;
    for(size_t i=0;i<arms.transforms.GetCount();++i){
        auto node=arms.transforms.GetEntity(i);nodes.push_back(node);
        auto h=arms.hierarchy.GetComponent(node);parents.push_back(h?h->parentID:0);
        if(!arms.names.Contains(node)||arms.names.GetComponent(node)->name.empty())
            arms.names.Create(node).name="Prefab node "+std::to_string(i);
    }
    // Record stable named anchors before exporting. Import may add a new transform root.
    auto sockets=melee?std::vector<SpawnPoint>{}:spawnPoints;if(!melee&&casingSocket)sockets.push_back(*casingSocket);
    json socketList=json::array();
    for(size_t i=0;i<sockets.size();++i){
        auto parent=mountedSocketParent(sockets[i]);requirePublish(parent!=0,"Missing PSP or casing parent: "+sockets[i].name);
        auto label="Renegade weapon anchor "+std::to_string(i);
        auto anchor=arms.Entity_CreateTransform(label);arms.Component_Attach(anchor,parent,true);
        auto socket=sockets[i];socket.part=LaunchSocketPart::Arms;socket.parentPath="";
        std::string encoded,error;requirePublish(SerializeLaunchSockets({socket},encoded,error),error);
        socketList.push_back({{"anchor",label},{"socket",json::parse(encoded)["sockets"][0]}});
    }
    recipe["anchors"]=socketList;recipe["actions"]=json::object();
    if(melee)recipe["melee"]=meleeDocument();
    auto view=XMMatrixLookAtLH(XMLoadFloat3(&eye),XMLoadFloat3(&eye)+XMLoadFloat3(&forward),assembled&&arms.cameras.GetCount()?XMLoadFloat3(&arms.cameras[0].Up):XMVectorSet(0,1,0,0));
    XMVECTOR scale,rot,pos;XMMatrixDecompose(&scale,&rot,&pos,view);
    XMFLOAT3 cp;XMFLOAT4 cq;XMStoreFloat3(&cp,pos);XMStoreFloat4(&cq,rot);
    recipe["cameraPosition"]={cp.x,cp.y,cp.z};recipe["cameraRotation"]={cq.x,cq.y,cq.z,cq.w};
    if(assembled){
        requirePublish(assembledActions.contains("Idle"),"Assign an Idle clip before publishing.");
        recipe["actions"]=assembledActions;
        for(size_t i=0;i<arms.animations.GetCount();++i)arms.animations[i].Stop();
        baked.Merge(arms);return;
    }
    std::vector<std::pair<std::string,AnimationComponent>> tracks;
    for(const auto& entry:preset.at("clips")){
        const std::string name=entry.at("source");int index=-1;
        for(size_t i=0;i<arms.animations.GetCount();++i)if(auto n=arms.names.GetComponent(arms.animations.GetEntity(i));n&&n->name==name){index=int(i);break;}
        if(index<0)continue;
        selectClip(index);auto& a=arms.animations[index];a.Pause();a.amount=1;
        const float length=a.end-a.start;if(!(length>0))continue;
        AnimationComponent track;track.end=length;track.Pause();
        using Path=AnimationComponent::AnimationChannel::Path;
        for(auto node:nodes)for(auto path:{Path::TRANSLATION,Path::ROTATION,Path::SCALE}){
            auto& ch=track.channels.emplace_back();ch.target=node;ch.path=path;ch.samplerIndex=int(track.samplers.size());
            auto& sampler=track.samplers.emplace_back();sampler.data=wi::ecs::CreateEntity();arms.animation_datas.Create(sampler.data);
        }
        int steps=std::max(1,int(std::ceil(length*60.f)));
        for(int frame=0;frame<=steps;++frame){
            float t=length*frame/steps;fingerFit.restoreBase();armFit.restoreBase();
            a.timer=a.start+t;a.last_update_time=-1;syncPreviewItem();arms.Update(0);
            fingerFit.captureBase();armFit.phase=posePhase();armFit.clip=name;armFit.captureBase();armFit.targets(false);armFit.solve(2);
            boltContact.apply(posePhase(),name=="Reload");armFit.hierarchy();solveSupport(false);applyFingers();
            updateMagazineAttachment(a.timer);armFit.hierarchy();
            for(size_t n=0;n<nodes.size();++n){
                auto& tr=*arms.transforms.GetComponent(nodes[n]);auto transform=tr.GetLocalMatrix();
                if(nodes[n]==magazineBone && magazineHeld && parents[n])
                    transform=tr.GetWorldMatrix()*XMMatrixInverse(nullptr,arms.transforms.GetComponent(parents[n])->GetWorldMatrix());
                XMVECTOR sv,qv,pv;requirePublish(XMMatrixDecompose(&sv,&qv,&pv,transform),"Cannot bake non-decomposable weapon pose.");
                XMFLOAT4 values[3];XMStoreFloat4(&values[0],pv);XMStoreFloat4(&values[1],qv);XMStoreFloat4(&values[2],sv);
                for(int k=0;k<3;++k){
                    auto& d=*arms.animation_datas.GetComponent(track.samplers[3*n+k].data);
                    d.keyframe_times.push_back(t);auto& v=values[k];
                    if(k==1 && d.keyframe_data.size()>=4){
                        const float* prev=&d.keyframe_data[d.keyframe_data.size()-4];
                        if(prev[0]*v.x+prev[1]*v.y+prev[2]*v.z+prev[3]*v.w<0)v={-v.x,-v.y,-v.z,-v.w};
                    }
                    d.keyframe_data.push_back(v.x);d.keyframe_data.push_back(v.y);d.keyframe_data.push_back(v.z);
                    if(k==1)d.keyframe_data.push_back(v.w);
                }
            }
        }
        for(auto& ch:track.channels){
            auto& data=*arms.animation_datas.GetComponent(track.samplers[ch.samplerIndex].data);
            size_t stride=ch.path==Path::ROTATION?4:3;
            auto& keys=data.keyframe_data;auto& times=data.keyframe_times;
            bool constant=true;
            for(size_t j=stride;j<keys.size()&&constant;++j)constant=std::abs(keys[j]-keys[j%stride])<1e-6f;
            if(constant&&times.size()>2){
                times[1]=times.back();times.resize(2);keys.resize(stride*2);
            }
        }
        attachMagazine(false);auto semantic=prefabAction(entry.value("label",name));if(semantic.empty())semantic=prefabAction(name);
        if(melee){
            if(name==attackClip)semantic="Attack";else if(name==restClip)semantic="Idle";
            else if(semantic!="Walk"&&semantic!="Run"&&semantic!="Equip"&&semantic!="Unequip")semantic.clear();
        }
        if(!semantic.empty())recipe["actions"][semantic]=name;
        if(melee)for(size_t slot=0;slot<extraMeleeClips.size();++slot)if(extraMeleeClips[slot]==name)recipe["actions"][meleeRoles[slot]]=name;
        tracks.emplace_back(name,std::move(track));
    }
    if(melee){
        for(size_t i=0;i<extraMeleeClips.size();++i)if(!extraMeleeClips[i].empty())
            requirePublish(recipe["actions"].contains(meleeRoles[i]),"Assigned melee clip is missing: "+extraMeleeClips[i]);
        requirePublish((extraMeleeClips[2].empty()&&extraMeleeClips[4].empty())||!extraMeleeClips[3].empty(),"Choose a held block clip to use guard transitions.");
    }
    if(melee)requirePublish(!attackClip.empty()&&restClip!=attackClip&&recipe["actions"].contains("Attack"),"Choose distinct idle and attack clips in Melee attack.");
    requirePublish(recipe["actions"].contains("Idle"),"An Idle animation is required for Starting Equipment.");
    arms.animations.Clear();std::set<Entity> retained;
    for(auto& t:tracks){
        auto id=wi::ecs::CreateEntity();arms.names.Create(id).name=t.first;
        for(auto& sampler:t.second.samplers)retained.insert(sampler.data);
        arms.animations.Create(id)=std::move(t.second);
    }
    for(size_t i=arms.animation_datas.GetCount();i-->0;){auto id=arms.animation_datas.GetEntity(i);if(!retained.count(id))arms.animation_datas.Remove(id);}
    baked.Merge(arms);
}

void Preview::makeWeaponPrefab(const fs::path& dest){
    requirePublish(assembled||(hand&&instance),"Fit the weapon to the arms before publishing.");
    PublishScratch scratch;auto tmp=scratch.path;
    json package={{"format","renegade.weaponprefab"},{"version",1},{"files",json::object()}};
    auto pack=[&](const std::string& key,const fs::path& file){package["files"][key]=json::binary(prefabBytes(file));};
    auto setup=describeSetup(tmp/"setup.weapon");auto editPreset=preset;
    auto armsSource=requestedArms.empty()?presetFile.parent_path()/fs::u8path(preset.at("model").get<std::string>()):requestedArms;
    Scene original;readModel(armsSource,original);saveNativePart(original,tmp/"arms.wiscene");pack("arms.wiscene",tmp/"arms.wiscene");original.Clear();
    if(!assembled){readModel(model,original);saveNativePart(original,tmp/"weapon.wiscene");pack("weapon.wiscene",tmp/"weapon.wiscene");original.Clear();}
    editPreset["model"]="arms.wiscene";for(auto key:{"projectSetup","setup","weapon"})editPreset.erase(key);
    setup.erase("armsPreset");setup.erase("armsModel");setup["localArmsPreset"]="preset.json";setup["model"]=setup.value("assembled",false)?"arms.wiscene":"weapon.wiscene";
    package["preset"]=editPreset;
    std::map<fs::path,std::string> dependencies;
    auto addFx=[&](const fs::path& path){
        auto key=fs::weakly_canonical(path);auto it=dependencies.find(key);if(it!=dependencies.end())return it->second;
        auto local="fx-"+std::to_string(dependencies.size())+path.extension().u8string();
        pack(local,path);dependencies.emplace(key,local);return local;
    };
    if(setup.contains("barrelEffects"))for(auto& effect:setup["barrelEffects"])if(effect.contains("file"))effect["file"]=addFx(projectRoot/fs::u8path(effect["file"].get<std::string>()));
    if(!melee&&!shotSound.empty())setup["shotSound"]=addFx(projectRoot/fs::u8path(shotSound));
    if(setup.contains("animationSounds"))for(auto& clip:setup["animationSounds"])for(auto& cue:clip)
        cue[1]=addFx(projectRoot/fs::u8path(cue[1].get<std::string>()));
    if(setup.contains("muzzleFlashPreview")){
        const char* styles[]={"Compact.png","Star.png","Long.png"};
        setup["muzzleFlashPreview"]["texture"]=addFx(flashTexture.empty()?home/"FlashEffects"/styles[flashStyle]:projectRoot/fs::u8path(flashTexture));
    }
    if(!melee&&!casingAsset.empty()){
        Scene casing;readModel(casingAsset,casing);ExportModel_GLTF((tmp/"casing.glb").u8string(),casing);
        pack("casing.glb",tmp/"casing.glb");setup["casingPreview"]["model"]="casing.glb";
    }
    auto equipmentId=linkedItem;
    if(equipmentId.empty()&&preset.contains("projectSetup"))
        equipmentId=equipmentForSetup(projectRoot/fs::u8path(preset.at("projectSetup").get<std::string>()));
    if(!melee&&(!equipmentId.empty()||!firingDraft.is_null())){
        std::string error,encoded;EquipmentAssetDocument doc;
        if(!equipmentId.empty()){
            requirePublish(ReadEquipmentAssetFile((projectRoot/"Content/Player/Equipment"/(equipmentId+".requipment")).u8string(),doc,error),error);
            FirstPersonAssemblySettings source;
            requirePublish(FirstPersonAssemblyService().ReadSettings(projectRoot.u8string(),doc.projectId,doc.equipment.presentationAssetId,source,error),error);
            package["firearm"]={source.firearm.capacity,source.firearm.minimumShotInterval,source.firearm.allowPartialReload};
            applyFiring(doc.equipment);
            requirePublish(SerializeEquipmentAsset(doc,encoded,error),error);package["equipment"]=json::parse(encoded);
        }else{
            resolveFiringProject();doc.projectId=firingProject.projectId;applyFiring(doc.equipment);
            const auto& v=doc.equipment.projectiles.front();
            package["newBinding"]={{"mode",int(v.fireMode)},{"projectile",v.projectileAssetId},{"socket",v.launchSocketName},{"second",v.secondSocketName},{"policy",v.socketPolicy},{"time",v.releaseSeconds}};
        }
        for(auto& binding:doc.equipment.projectiles){
            auto id=binding.projectileAssetId;if(id.empty()||package["projectiles"].contains(id))continue;
            ProjectileAssetDocument projectile;requirePublish(LoadProjectileAsset(projectRoot.u8string(),doc.projectId,id,projectile,error),error);
            if(!projectile.meshAssetId.empty()){
                auto placed=ReusableAssetService().PrepareModelAssetPlacement({projectRoot.u8string(),doc.projectId,projectile.meshAssetId});
                requirePublish(placed.IsReady(),placed.Result().error);
                auto key="projectile-"+id+".glb";ExportModel_GLTF((tmp/key).u8string(),*placed.PeekMutableScene());pack(key,tmp/key);
            }
            requirePublish(SerializeProjectileAsset(projectile,encoded,error),error);package["projectiles"][id]=json::parse(encoded);
        }
    }
    package["setup"]=setup;
    // Use a private editor so baking cannot change the current selection, pose or playhead.
    auto privateSetup=tmp/"working.weapon";std::ofstream(privateSetup)<<describeSetup(privateSetup).dump();
    struct RestoreSelection {fs::path saved=requestedSetup;~RestoreSelection(){requestedSetup=saved;}} restore;
    requestedSetup=privateSetup;auto preview=std::make_unique<Preview>();preview->init(*this);preview->Load();
    Scene baked;preview->bakePrefab(baked,package["recipe"]);
    exportWeaponPresentation(tmp/"presentation.glb",baked);pack("presentation.glb",tmp/"presentation.glb");
    auto bytes=json::to_cbor(package);writePrefabBytes(dest,bytes);
}
namespace {
FirstPersonAssemblySettings publishedSettings(ModelImportCandidate& candidate,const json& package){
    std::string error;FirstPersonAssemblySettings settings;
    auto& scene=*candidate.PeekMutableScene();settings.integratedWeaponPresentation=true;
    std::vector<PlayerViewBoneChoice> bones;requirePublish(CollectPlayerViewBones(scene,bones,error)&&!bones.empty(),"Published rig has no bones.");
    settings.parentBonePath=bones.front().path;auto& recipe=package.at("recipe");
    if(recipe.contains("melee")){
        const auto& m=recipe.at("melee");MeleeWeaponSettings strike{m.at("reach").get<float>(),m.at("damage").get<float>(),m.at("contact").get<float>()};
        requirePublish(strike.Valid(),"Invalid melee settings in prefab.");
        auto id=bones.front().entity;auto meta=scene.metadatas.GetComponent(id);if(!meta)meta=&scene.metadatas.Create(id);SetMeleeWeapon(*meta,strike);
    }
    auto cp=recipe.at("cameraPosition").get<std::array<float,3>>();settings.cameraPosition={cp[0],cp[1],cp[2]};
    auto cq=recipe.at("cameraRotation").get<std::array<float,4>>();settings.cameraRotation={cq[0],cq[1],cq[2],cq[3]};
    if(package.contains("firearm")){
        auto& f=package.at("firearm");settings.firearm={f.at(0).get<int>(),f.at(1).get<float>(),f.at(2).get<bool>()};
        requirePublish(ValidateFirearmSettings(settings.firearm,error),error);
    }
    auto anchors=CollectLaunchSocketParents(scene);
    for(auto& anchor:recipe.at("anchors")){
        auto id=scene.Entity_FindByName(anchor.at("anchor").get<std::string>());auto node=std::find_if(anchors.begin(),anchors.end(),[&](auto& n){return n.entity==id;});
        requirePublish(node!=anchors.end(),"Published socket anchor could not be resolved.");
        auto socket=anchor.at("socket");socket["parent_path"]=node->path;
        std::vector<LaunchSocketDefinition> parsed;requirePublish(ParseLaunchSockets(json({{"schema_version",1},{"sockets",json::array({socket})}}).dump(),parsed,error),error);
        parsed.front().parentPath=node->path;settings.launchSockets.push_back(parsed.front());
    }
    for(auto it=recipe.at("actions").begin();it!=recipe.at("actions").end();++it){
        bool added=false;
        for(size_t i=0;i<scene.animations.GetCount();++i){
            auto n=scene.names.GetComponent(scene.animations.GetEntity(i));if(n&&n->name==it.value().get<std::string>()){settings.pairs.push_back({it.key(),unsigned(i),0});added=true;break;}
        }
        requirePublish(added,"Published animation is missing: "+it.key());
    }
    return settings;
}
StableId installWeaponPrefab(const fs::path& project,const fs::path& file){
    const auto name=file.stem().u8string();requirePublish(!name.empty()&&name.size()<70&&name!="."&&name!=".."&&name.find_first_of("<>:\"/\\|?*")==std::string::npos,"Choose a valid prefab name.");
    auto package=json::from_cbor(prefabBytes(file));requirePublish(package.value("format",std::string{})=="renegade.weaponprefab"&&package.value("version",0)==1,"Unsupported weapon prefab.");
    PublishScratch scratch;const auto stage=scratch.path/"Project";const auto relative=fs::path("Content/Player/Weapons")/fs::u8path(name);const auto parts=stage/relative;
    fs::create_directories(parts);
    auto folder=relative.generic_u8string()+"/";
    auto& files=package.at("files");
    auto leaf=[&](const std::string& key){
        return !key.empty() && key!="."&&key!=".."&&key.find_first_of("\\/:<>|?*\"")==std::string::npos&&files.contains(key);
    };
    for(auto it=files.begin();it!=files.end();++it){
        requirePublish(leaf(it.key())&&it.value().is_binary(),"Invalid weapon prefab dependency.");
        writePrefabBytes(parts/fs::u8path(it.key()),it.value().get_binary());
    }
    std::string error;ProjectMetadata meta;
    for(auto& p:fs::directory_iterator(project))if(p.path().extension()==".renegade"&&ProjectService().InspectProject(p.path().u8string(),meta,error))break;
    requirePublish(!meta.projectId.empty(),"Open a Renegade project before publishing.");
    auto setup=package.at("setup");setup.erase("primaryFiring");setup["model"]=setup.value("assembled",false)?"arms.wiscene":"weapon.wiscene";setup.erase("armsModel");setup.erase("armsPreset");setup["localArmsPreset"]="preset.json";
    auto preset=package.at("preset");preset["model"]="arms.wiscene";for(auto key:{"weapon","setup","projectSetup"})preset.erase(key);
    auto resource=[&](json& value,bool projectRelative){
        auto path=value.get<std::string>();requirePublish(leaf(path),"Missing prefab dependency: "+path);if(projectRelative)value=folder+path;
    };
    if(setup.contains("barrelEffects"))for(auto& effect:setup["barrelEffects"])if(effect.contains("file"))resource(effect["file"],true);
    if(setup.contains("shotSound"))resource(setup["shotSound"],true);
    if(setup.contains("animationSounds"))for(auto& clip:setup["animationSounds"])for(auto& cue:clip)resource(cue.at(1),true);
    if(setup.contains("muzzleFlashPreview"))resource(setup["muzzleFlashPreview"].at("texture"),true);
    if(setup.contains("casingPreview")&&!setup["casingPreview"].value("model",std::string{}).empty())resource(setup["casingPreview"]["model"],false);
    std::ofstream(parts/"setup.weapon")<<setup.dump(2);std::ofstream(parts/"preset.json")<<preset.dump(2);
    auto candidate=ModelImportCandidateService().PrepareModel((parts/"presentation.glb").u8string());requirePublish(candidate.IsReady(),candidate.Error());
    auto settings=publishedSettings(candidate,package);
    ModelImportCommitRequest request;request.projectRoot=stage.u8string();request.projectId=meta.projectId;request.assetName=name+" Presentation";request.destinationFolder="Content/Models/WeaponPresentations";
    auto saved=ModelImportCommitService().CommitModel(request,candidate,{},true);requirePublish(saved.succeeded,saved.error);settings.armsAssetId=saved.assetId;
    FirstPersonAssemblyService service;StableId assembly;requirePublish(service.Save(stage.u8string(),meta.projectId,name,settings,{},assembly,error),error);
    EquipmentDefinition item;requirePublish(PrepareEquipmentFromAssembly(stage.u8string(),meta.projectId,assembly,name,EquipmentHandUse::PrimaryWithSupport,item,error),error);
    if(package.contains("equipment")||package.contains("newBinding")){
      if(package.contains("equipment")){
        EquipmentAssetDocument source;requirePublish(DeserializeEquipmentAsset(package.at("equipment").dump(),source,error),error);
        item.handUse=source.equipment.handUse;item.actions=source.equipment.actions;item.projectiles=source.equipment.projectiles;
      }else{
        const auto& f=package.at("newBinding");EquipmentProjectileBinding v;v.fireMode=static_cast<EquipmentFireMode>(f.at("mode").get<int>());v.projectileAssetId=f.at("projectile");v.launchSocketName=f.at("socket");v.secondSocketName=f.at("second");v.socketPolicy=f.at("policy");v.releaseSeconds=f.at("time");item.projectiles.push_back(v);
      }
        std::map<StableId,StableId> remap;
        for(auto& binding:item.projectiles){
            auto id=binding.projectileAssetId;if(id.empty())continue;
            if(!remap.count(id)){
                ProjectileAssetDocument p;requirePublish(DeserializeProjectileAsset(package.at("projectiles").at(id).dump(),p,error),error);
                if(!p.meshAssetId.empty()){
                    auto key="projectile-"+id+".glb";requirePublish(leaf(key),"Missing projectile model.");
                    auto mesh=ModelImportCandidateService().PrepareModel((parts/key).u8string());requirePublish(mesh.IsReady(),mesh.Error());
                    auto requestMesh=request;requestMesh.assetName=name+" Projectile "+std::to_string(remap.size()+1);
                    auto model=ModelImportCommitService().CommitModel(requestMesh,mesh,{},true);requirePublish(model.succeeded,model.error);p.meshAssetId=model.assetId;
                }
                p.assetId.clear();p.projectId=meta.projectId;auto projectile=SaveProjectileAsset(stage.u8string(),meta.projectId,p);
                requirePublish(projectile.succeeded,projectile.error);remap[id]=projectile.document.assetId;
            }
            binding.projectileAssetId=remap.at(id);
        }
    }
    if(package.at("recipe").contains("melee")){
        item.handUse=package["recipe"]["melee"].value("twoHands",false)?EquipmentHandUse::TwoHanded:EquipmentHandUse::PrimaryOnly;
        item.projectiles.clear();
    }
    auto equipment=SaveEquipmentAsset(stage.u8string(),meta.projectId,item);requirePublish(equipment.succeeded,equipment.error);
    auto slots=stage/WeaponLoadoutPath(equipment.document.equipment.assetId);fs::create_directories(slots.parent_path());std::ofstream(slots)<<equipment.document.equipment.assetId<<'\t'<<folder<<"setup.weapon\n";
    AssetRegistry registry,added;requirePublish(ReadAssetRegistry(stage.u8string(),meta.projectId,added,error),error);
    if(fs::exists(project/AssetRegistryDocumentName))requirePublish(ReadAssetRegistry(project.u8string(),meta.projectId,registry,error),error);else registry.projectId=meta.projectId;
    registry.records.insert(registry.records.end(),added.records.begin(),added.records.end());registry.importedProducts.insert(registry.importedProducts.end(),added.importedProducts.begin(),added.importedProducts.end());
    registry.schemaVersion=AssetRegistry::CurrentSchemaVersion;std::string registryText;requirePublish(SerializeAssetRegistry(registry,registryText,error),error);
    AssetCatalogueMetadataDocument metadata,extra;requirePublish(ReadAssetCatalogueMetadata(project.u8string(),meta.projectId,metadata,error),error);requirePublish(ReadAssetCatalogueMetadata(stage.u8string(),meta.projectId,extra,error),error);
    metadata.records.insert(metadata.records.end(),extra.records.begin(),extra.records.end());std::string metadataText;requirePublish(SerializeAssetCatalogueMetadata(metadata,metadataText,error),error);
    std::vector<ProjectDocumentWrite> writes;
    for(auto dir:{"Content","SourceAssets"})for(const auto& entry:fs::recursive_directory_iterator(stage/dir))if(entry.is_regular_file()){
        const auto target=project/entry.path().lexically_relative(stage);
        requirePublish(!fs::exists(target),"There is already an asset at "+target.u8string()+". Choose another prefab name.");
        fs::create_directories(target.parent_path());writes.push_back({target.u8string(),prefabBytes(entry.path())});
    }
    writes.push_back({(project/AssetRegistryDocumentName).u8string(),{registryText.begin(),registryText.end()}});
    writes.push_back({(project/AssetCatalogueMetadataDocumentName).u8string(),{metadataText.begin(),metadataText.end()}});
    ProjectDocumentTransactionOptions options;options.allowedRoot=project.u8string();options.journalDirectory=(project/"Intermediate/Transactions").u8string();
    auto commit=ProjectDocumentTransaction().Execute(std::move(writes),options);requirePublish(commit.success,commit.message);
    return equipment.document.equipment.assetId;
}
}
void Preview::publishWeaponDialog(){
    auto file=choose(true,L"Portable weapon prefab (*.weaponprefab)\0*.weaponprefab\0\0",L"weaponprefab",projectRoot);
    if(file.empty())return;
    try{
        PublishScratch scratch;auto temp=scratch.path/file.filename();makeWeaponPrefab(temp);
        // Install first; a rejected publication cannot overwrite a previous export.
        auto id=installWeaponPrefab(projectRoot,temp);auto slots=ReadWeaponSlots(projectRoot,id);if(!slots.empty())linkEquipment(projectRoot/fs::u8path(slots.front().setup));
        fs::copy_file(temp,file,fs::copy_options::overwrite_existing);
        status.SetText("Published "+file.stem().u8string()+". Select it in Player Start > Starting Equipment.");
    }catch(const std::exception& ex){status.SetText(ex.what());MessageBoxA(editorWindow,ex.what(),"Save Weapon to Library",MB_OK);}
}
