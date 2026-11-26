wi::ecs::Entity RuntimeWeaponEffects::Mesh(Scene& scene,const std::filesystem::path& path,bool casing){
    using namespace wi::scene;
    auto key=path.u8string();auto found=meshes.find(key);if(found!=meshes.end())return found->second;
    if(!std::filesystem::is_regular_file(path))return 0;
    if(casing){
        auto candidate=bridge::ModelImportCandidateService().PrepareModel(key);
        auto source=candidate.PeekMutableScene();if(!source||!source->objects.GetCount())return 0;
        auto mesh=source->objects[0].meshID;scene.meshes.Merge(source->meshes);scene.materials.Merge(source->materials);
        meshes.emplace(key,mesh);return mesh;
    }
    auto texture=wi::resourcemanager::Load(key);if(!texture.IsValid())return 0;
    Entity e=wi::ecs::CreateEntity();
    auto& mat=scene.materials.Create(e);
    mat.shaderType=MaterialComponent::SHADERTYPE_UNLIT;
    mat.userBlendMode=wi::enums::BLENDMODE_ADDITIVE;
    mat.baseColor={1.f,.76f,.37f,1};mat.SetCastShadow(false);
    auto& map=mat.textures[MaterialComponent::BASECOLORMAP];map.name=path.u8string();map.resource=texture;
    auto& mesh=scene.meshes.Create(e);mesh.SetDoubleSided(true);
    auto quad=[&](XMFLOAT3 a,XMFLOAT3 b,XMFLOAT3 c,XMFLOAT3 d,XMFLOAT3 normal){
        uint32_t n=(uint32_t)mesh.vertex_positions.size();
        for(auto p:{a,b,c,d})mesh.vertex_positions.push_back(p);
        for(auto uv:{XMFLOAT2{0,1},XMFLOAT2{1,1},XMFLOAT2{1,0},XMFLOAT2{0,0}})mesh.vertex_uvset_0.push_back(uv);
        for(int i=0;i<4;++i)mesh.vertex_normals.push_back(normal);
        for(auto i:{0,1,2,0,2,3})mesh.indices.push_back(n+i);
    };
    // The bright base of each side texture lies about 14% above its bottom edge.
    quad({-.10f,0,-.034f},{.10f,0,-.034f},{.10f,0,.21f},{-.10f,0,.21f},{0,-1,0});
    quad({0,-.10f,-.034f},{0,.10f,-.034f},{0,.10f,.21f},{0,-.10f,.21f},{1,0,0});
    quad({-.043f,-.043f,.017f},{.043f,-.043f,.017f},{.043f,.043f,.017f},{-.043f,.043f,.017f},{0,0,1});
    auto& subset=mesh.subsets.emplace_back();subset.materialID=e;subset.indexCount=uint32_t(mesh.indices.size());
    mesh.CreateRenderData();meshes.emplace(key,e);return e;
}
void RuntimeApplication::ApplyWeaponMaterials(const std::string& setup){
    if(setup.empty()||!playerViewRig_.viewModelRoot)return;
    std::ifstream file(std::filesystem::u8path(startupResult_.project.rootPath)/std::filesystem::u8path(setup));
    if(!file)return;
    auto doc=nlohmann::json::parse(file,nullptr,false);
    if(!doc.is_object() || !doc.contains("materials"))return;
    auto& scene=scenes_.GetScene();
    for(size_t i=0;i<scene.objects.GetCount();++i){
        auto entity=scene.objects.GetEntity(i);
        if(!scene.Entity_IsDescendant(entity,playerViewRig_.viewModelRoot))continue;
        bool weapon=false;
        for(auto ancestor=entity;ancestor && ancestor!=playerViewRig_.viewModelRoot;){
            auto n=scene.names.GetComponent(ancestor);
            if(n && (n->name=="weapon_mount" || n->name=="hand_r"))weapon=true;
            auto h=scene.hierarchy.GetComponent(ancestor);ancestor=h?h->parentID:0;
        }
        auto mesh=scene.meshes.GetComponent(scene.objects[i].meshID);if(!mesh)continue;
        for(size_t j=0;j<mesh->subsets.size();++j){
            auto id=mesh->subsets[j].materialID;auto mat=scene.materials.GetComponent(id);if(!mat)continue;
            auto n=scene.names.GetComponent(id);
            auto name=(n&&!n->name.empty())?n->name:"Material "+std::to_string(j+1);
            auto& rows=doc["materials"];const auto entry=rows.find(std::string(weapon?"Weapon / ":"Arms / ")+name);
            if(entry==rows.end() || !entry->is_array() || entry->size()!=3)continue;
            bridge::WeaponSurface values{};bool valid=true;
            for(int k=0;k<3;++k){if(!(*entry)[k].is_number()){valid=false;break;}values[k]=(*entry)[k].get<float>();if(!std::isfinite(values[k])||values[k]<0||values[k]>1)valid=false;}
            if(valid){
                bridge::SetWeaponSurface(*mat,values);
                const auto maps=doc.find("disabledSurfaceMaps");
                if(maps!=doc.end()&&maps->is_array()&&std::find(maps->begin(),maps->end(),entry.key())!=maps->end()){
                    mat->textures[wi::scene::MaterialComponent::SURFACEMAP]={};mat->SetDirty();
                }
            }
        }
    }
}
void RuntimeApplication::SelectWeaponSlot(size_t index){
    if(index>=weaponSlots_.size() || index==activeWeaponSlot_ || activeWeaponSlot_>=preparedWeapons_.size())return;
    auto& scene=scenes_.GetScene();
    auto& current=preparedWeapons_[activeWeaponSlot_];
    auto& target=preparedWeapons_[index];
    auto settings=playerSettings_;
    settings.primaryEquipmentAssetId=weaponSlots_[index].equipment;
    const auto& project=startupResult_.project;
    std::string error;
    if(!target.ready){
        if(!target.equipment.Load(project.rootPath,project.projectId,settings))return;
        const auto asset=target.equipment.Presentation("");
        target.rig=playerViewRig_;
        wi::unordered_set<wi::ecs::Entity> before;
        scene.FindAllEntities(before);
        bool loaded=startupResult_.packageRelativeLaunch?
            LoadPackagedRuntimePlayerViewAsset(scene,target.rig,startupResult_.packageRootPath,project.projectId,asset,error):
            LoadRuntimePlayerViewAsset(scene,target.rig,project.rootPath,project.projectId,asset,error);
        target.entities.clear();
        scene.FindAllEntities(target.entities);
        for(auto entity:before)target.entities.erase(entity);
        if(!loaded || !InitializeRuntimePlayerViewAnimations(scene,target.rig,target.animation,error)){
            (void)BindRuntimePlayerViewRigSockets(scene,playerViewRig_,playerViewRig_.viewModelRoot,error);
            for(auto entity:target.entities)scene.Entity_Remove(entity,false);
            target={};
            return;
        }
        weaponEffects_.Prepare(scene,target.rig.viewModelRoot,std::filesystem::u8path(project.rootPath),weaponSlots_[index].setup,target.shotProfile);
        target.ready=true;
        for(const auto& request:target.equipment.resolvedProjectiles)
            if(request.fireMode==bridge::EquipmentFireMode::Projectile)
                projectileVisuals_.Prepare(project.rootPath,startupResult_.packageRelativeLaunch?startupResult_.packageRootPath:"",project.projectId,request.projectile);
    }else if(!BindRuntimePlayerViewRigSockets(scene,target.rig,target.rig.viewModelRoot,error))return;
    current.equipment=std::move(playerEquipment_);
    slotAmmo_[activeWeaponSlot_]=playerViewAnimation_.loadedShells;
    ResetRuntimePlayerViewAnimations(scene,playerViewAnimation_);
    for(auto entity:current.entities){
        if(auto* object=scene.objects.GetComponent(entity)){
            current.parkedObjects.emplace_back(entity,std::move(*object));
            scene.objects.Remove(entity);
        }
    }
    for(auto& [entity,object]:target.parkedObjects)scene.objects.Create(entity)=std::move(object);
    target.parkedObjects.clear();
    auto animation=target.animation;
    if(slotAmmo_[index]>=0)animation.loadedShells=std::min(slotAmmo_[index],int(animation.firearm.capacity));
    playerViewRig_=target.rig;
    playerViewAnimation_=std::move(animation);
    playerEquipment_=std::move(target.equipment);
    settings.firstPersonArmsAssetId=target.rig.viewModelAssetId;
    playerSettings_=settings;
    activeWeaponSlot_=index;
    renderer_.SetProjectileAim(playerEquipment_.ready && !playerEquipment_.resolvedProjectiles.empty());
    if(slotAmmo_[index]<0)ApplyWeaponMaterials(weaponSlots_[index].setup);
    (void)RequestRuntimePlayerViewAnimation(scene,playerViewAnimation_,PlayerViewAction::Idle);
}
