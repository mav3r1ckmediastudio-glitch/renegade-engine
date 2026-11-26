void Preview::updateLibraryWeapon(){
    using namespace renegade::bridge;
    {
        requirePublish(!linkedItem.empty()&&!linkedSetup.empty(),"Open the library weapon first.");
        auto root=projectRoot;auto project=firingProject.projectId;
        AssetRegistry initial;std::string error;
        requirePublish(ReadAssetRegistry(root.u8string(),project,initial,error),error);
        auto record=[&](const StableId& id)->const AssetRecord&{
            auto it=std::find_if(initial.records.begin(),initial.records.end(),[&](auto& r){return r.assetId==id;});
            if(it==initial.records.end())throw std::runtime_error("A weapon dependency is missing.");return *it;
        };
        auto& existing=record(linkedItem);requirePublish(existing.contentHash==linkedHash,"This weapon changed. Reopen it before saving.");
        EquipmentAssetDocument doc;requirePublish(LoadEquipmentAsset(root.u8string(),project,linkedItem,doc,error),error);
        applyFiring(doc.equipment);
        auto& assembly=record(doc.equipment.presentationAssetId);FirstPersonAssemblyService service;
        requirePublish(assembly.contentHash==linkedPresentationHash,"The weapon fit changed. Reopen it before saving.");
        FirstPersonAssemblySettings original;requirePublish(service.ReadSettings(root.u8string(),project,assembly.assetId,original,error),error);
        PublishScratch scratch;auto stage=scratch.path/"Project";fs::create_directories(stage);
        auto setup=describeSetup(linkedSetup);setup.erase("primaryFiring");
        // A digest of the authored pose lets later firing-only edits reuse the saved visual.
        auto digest=[](const std::string& bytes){uint64_t value=1469598103934665603ull;for(unsigned char c:bytes){value^=c;value*=1099511628211ull;}std::ostringstream result;result<<std::hex<<value;return result.str();};
        auto visual=setup;for(auto key:{"barrelEffects","automaticFire","loopShotSound","animationSounds","shotSound","muzzleFlashPreview","casingPreview"})visual.erase(key);
        auto armsFile=requestedArms.empty()&&preset.contains("model")?presetFile.parent_path()/fs::u8path(preset.at("model").get<std::string>()):requestedArms;
        for(auto path:{model,presetFile,armsFile})if(!path.empty()&&fs::is_regular_file(path))visual["sourceRevisions"][path.u8string()]=fs::last_write_time(path).time_since_epoch().count();
        visual["materialExportVersion"]=2;
        const auto signature=digest(visual.dump());json previous;std::ifstream(linkedSetup)>>previous;
        setup["publishedPose"]=signature;
        auto storeSetup=[&](){auto text=setup.dump(2);return ProjectDocumentWrite{linkedSetup.u8string(),{text.begin(),text.end()}};};
        if(previous.value("publishedPose",std::string{})==signature){
            auto saved=UpdateEquipmentAsset(root.u8string(),project,doc.equipment,linkedHash,{storeSetup()});requirePublish(saved.succeeded,saved.error);
        }else{
            // All native services write to an isolated registry before the live transaction.
            auto registryBefore=prefabBytes(root/AssetRegistryDocumentName);
            auto metadataBefore=prefabBytes(root/AssetCatalogueMetadataDocumentName);
            writePrefabBytes(stage/AssetRegistryDocumentName,registryBefore);writePrefabBytes(stage/AssetCatalogueMetadataDocumentName,metadataBefore);
            std::set<fs::path> copied;
            auto stageRecord=[&](const AssetRecord& r){auto rel=fs::u8path(r.projectRelativePath);if(copied.insert(rel).second)writePrefabBytes(stage/rel,prefabBytes(root/rel));};
            stageRecord(assembly);stageRecord(existing);
            for(const auto& p:doc.equipment.projectiles)if(!p.projectileAssetId.empty()){
                stageRecord(record(p.projectileAssetId));ProjectileAssetDocument projectile;
                requirePublish(LoadProjectileAsset(root.u8string(),project,p.projectileAssetId,projectile,error),error);
                if(!projectile.meshAssetId.empty())stageRecord(record(projectile.meshAssetId));
            }
            auto working=scratch.path/"working.weapon";std::ofstream(working)<<describeSetup(working).dump();
            struct Restore{fs::path was=requestedSetup;~Restore(){requestedSetup=was;}} restore;
            requestedSetup=working;auto editor=std::make_unique<Preview>();editor->init(*this);editor->Load();Scene baked;json package;
            editor->bakePrefab(baked,package["recipe"]);auto glb=scratch.path/"presentation.glb";exportWeaponPresentation(glb,baked);
            auto modelCandidate=ModelImportCandidateService().PrepareModel(glb.u8string());requirePublish(modelCandidate.IsReady(),modelCandidate.Error());
            auto settings=publishedSettings(modelCandidate,package);settings.firearm=original.firearm;
            ModelImportCommitRequest request;request.projectRoot=stage.u8string();request.projectId=project;
            request.destinationFolder="Content/Models/WeaponPresentations";request.assetName="Fit "+linkedItem+" "+signature;
            const auto modelPath=request.destinationFolder+"/"+request.assetName+".rasset";
            auto found=std::find_if(initial.records.begin(),initial.records.end(),[&](auto& r){return r.projectRelativePath==modelPath;});
            if(found!=initial.records.end()){stageRecord(*found);settings.armsAssetId=found->assetId;}
            else {auto added=ModelImportCommitService().CommitModel(request,modelCandidate,{},true);requirePublish(added.succeeded,added.error);settings.armsAssetId=added.assetId;}
            requirePublish(service.Update(stage.u8string(),project,assembly.assetId,assembly.contentHash,settings,{},error),error);
            if(melee||previous.value("weaponType",std::string{})=="melee"){
                EquipmentDefinition refreshed;auto grip=melee?(twoHandMelee?EquipmentHandUse::TwoHanded:EquipmentHandUse::PrimaryOnly):EquipmentHandUse::PrimaryWithSupport;
                requirePublish(PrepareEquipmentFromAssembly(stage.u8string(),project,assembly.assetId,doc.equipment.name,grip,refreshed,error),error);
                doc.equipment.actions=refreshed.actions;doc.equipment.handUse=grip;
            }
            auto saved=UpdateEquipmentAsset(stage.u8string(),project,doc.equipment,linkedHash);requirePublish(saved.succeeded,saved.error);
            std::vector<ProjectDocumentWrite> writes={storeSetup()};
            for(auto dir:{"Content","SourceAssets"})for(auto& entry:fs::recursive_directory_iterator(stage/dir))if(entry.is_regular_file()){
                auto rel=entry.path().lexically_relative(stage);
                if(copied.count(rel)&&rel!=fs::u8path(existing.projectRelativePath)&&rel!=fs::u8path(assembly.projectRelativePath))continue;
                fs::create_directories((root/rel).parent_path());writes.push_back({(root/rel).u8string(),prefabBytes(entry.path())});
            }
            requirePublish(registryBefore==prefabBytes(root/AssetRegistryDocumentName)&&metadataBefore==prefabBytes(root/AssetCatalogueMetadataDocumentName),"Project assets changed while saving. Reopen the weapon and retry.");
            for(auto file:{AssetRegistryDocumentName,AssetCatalogueMetadataDocumentName})writes.push_back({(root/file).u8string(),prefabBytes(stage/file)});
            ProjectDocumentTransactionOptions options;options.allowedRoot=root.u8string();options.journalDirectory=(root/"Intermediate/Transactions").u8string();
            auto commit=ProjectDocumentTransaction().Execute(std::move(writes),options);requirePublish(commit.success,commit.message);
        }
        auto updated=linkedSetup;linkEquipment(updated);setupPath=updated;firingDraft=nullptr;lastSaved=describeSetup(updated);
        status.SetText("Saved "+doc.equipment.name+". Existing player loadouts use this version.");
    }
}
void Preview::saveExistingWeapon(){
    try{updateLibraryWeapon();}catch(const std::exception& ex){status.SetText(ex.what());MessageBoxA(editorWindow,ex.what(),"Save weapon changes",MB_OK);}
}

void Preview::exportWeaponCopy(){
    auto file=choose(true,L"Weapon prefab (*.weaponprefab)\0*.weaponprefab\0\0",L"weaponprefab",projectRoot);
    if(file.empty())return;
    auto temp=file;temp+=L"."+fs::u8path(bridge::GenerateStableId()).wstring()+L".tmp";
    try{
        makeWeaponPrefab(temp);
        if(!MoveFileExW(temp.c_str(),file.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error("Cannot replace the selected prefab file.");
        status.SetText("Exported "+file.filename().u8string());
    }catch(const std::exception& ex){std::error_code ignored;fs::remove(temp,ignored);status.SetText(ex.what());}
}
