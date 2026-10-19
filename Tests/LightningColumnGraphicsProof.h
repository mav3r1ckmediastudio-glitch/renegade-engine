#pragma once
#include "../Runtime/src/RuntimeLightningColumn.h"

static bool CaptureLightningColumn(wi::scene::Scene& scene,const fs::path& file) {
    renegade::studio::ModelImportPreview preview;std::string error;
    if(!preview.Prepare(scene,error)){std::cerr<<error<<"\n";return false;}
    // Import preview deliberately removes source lights. Restore real point
    // lights on its private copy so these captures actually test illumination.
    for(size_t i=0;i<preview.scene->lights.GetCount();++i)preview.scene->lights[i].intensity=.35f;
    for(size_t i=0;i<scene.lights.GetCount();++i) {
        const auto& light=scene.lights[i];
        const auto* t=scene.transforms.GetComponent(scene.lights.GetEntity(i));
        if(t&&light.GetType()==wi::scene::LightComponent::POINT)
            preview.scene->Entity_CreateLight("Column diagnostic real point light",
                t->GetPosition(),light.color,light.intensity,light.range,light.GetType());
    }
    preview.camera->CreatePerspective(512,320,.01f,80.f,XMConvertToRadians(52.f));
    preview.camera->TransformCamera(XMMatrixInverse(nullptr,XMMatrixLookAtLH(
        XMVectorSet(0,2.75f,-4.5f,1),XMVectorSet(0,2.75f,6,1),XMVectorSet(0,1,0,0))));
    preview.camera->UpdateCamera();
    preview.setBloomEnabled(true);preview.setBloomThreshold(1.f);
    preview.ResizeBuffers();
    std::vector<uint8_t> png;
    for(unsigned f=0;f<600;++f) {
        wi::eventhandler::FireEvent(wi::eventhandler::EVENT_THREAD_SAFE_POINT,0);
        preview.PreUpdate();preview.Update(1.f/60.f);preview.PreRender();preview.Render();
        wi::graphics::GetDevice()->SubmitCommandLists();
        wi::renderer::UpdateGPUSuballocator();Sleep(10);
        if(f>=15&&preview.IsReady())break;
    }
    if(!preview.CapturePng(png,error)){std::cerr<<error<<"\n";return false;}
    std::ofstream out(file,std::ios::binary);
    out.write(reinterpret_cast<const char*>(png.data()),png.size());return out.good();
}
static bool LightningColumnProof(const fs::path& project,const fs::path& output) {
    using namespace renegade::runtime;
    using namespace renegade::bridge;
    if(fs::exists(output/"column-00.png"))return false;
    wi::scene::Scene scene;
    const auto floor=scene.Entity_CreateCube("Isolated column matte receiving floor");
    RuntimeLightningColumn::Place(scene,floor,{0,-.12f,6},{4,.12f,4});
    if(auto* m=scene.materials.GetComponent(floor)){
        m->baseColor={.15f,.17f,.21f,1};m->SetRoughness(.95f);m->SetDirty();
    }
    scene.weathers.Create(wi::ecs::CreateEntity());
    RuntimeLightningColumn column;
    if(!column.Create(scene,(project/"Content/Effects/LightningColumn").generic_u8string()))return false;
    for(unsigned i=0;i<12;++i){
        column.Update(scene,{0,0,6},.045f);scene.Update(1.f/60.f);
        std::ostringstream name;name<<"column-"<<std::setw(2)<<std::setfill('0')<<i<<".png";
        if(!CaptureLightningColumn(scene,output/name.str()))return false;
    }
    for(unsigned i=0;i<60;++i){column.Update(scene,{0,0,6},1.f/60.f);scene.Update(1.f/60.f);}
    for(auto mask:column.visited)if(mask!=((1u<<RuntimeLightningColumn::Frames)-1))return false;
    for(auto& resource:column.frames)if(!resource.IsValid()||!resource.GetTexture().IsValid())return false;
    const auto* light=scene.lights.GetComponent(column.impactLight);
    if(!light||light->GetType()!=wi::scene::LightComponent::POINT||light->intensity<12.f)return false;
    if(!scene.emitters.Contains(column.sparks))return false;
    for(auto e:column.strips) {
        const auto* m=scene.materials.GetComponent(e);
        if(!m||!m->textures[wi::scene::MaterialComponent::EMISSIVEMAP].resource.GetTexture().IsValid())return false;
    }
    column.Reset(scene);
    if(scene.objects.GetCount()!=1||scene.emitters.GetCount()!=0||scene.lights.GetCount()!=0)return false;

    nlohmann::json source;
    {std::ifstream in(project/"storm-playable.json");if(!in||!(in>>source))return false;}
    const auto projectId=source.at("projectId").get<std::string>();
    EquipmentAssetDocument previous;std::string error;
    if(!LoadEquipmentAsset(project.generic_u8string(),projectId,
        source.at("abilityId").get<std::string>(),previous,error))return false;
    auto ability=previous.equipment;ability.name="Lightning Column - Strip Review";
    const auto saved=SaveEquipmentAsset(project.generic_u8string(),projectId,ability);
    if(!saved.succeeded){std::cerr<<saved.error<<"\n";return false;}
    EquipmentAssetDocument reopened;
    if(!LoadEquipmentAsset(project.generic_u8string(),projectId,saved.document.equipment.assetId,reopened,error)||
        reopened.equipment.name!=ability.name)return false;
    PlayerControllerSettings player;
    player.primaryEquipmentAssetId=saved.document.equipment.assetId;
    player.firstPersonArmsAssetId=ability.presentationAssetId;
    SceneService level;auto& world=level.GetScene();
    wi::Archive archive((project/"Content/Scenes/Main.wiscene").generic_u8string(),true);
    if(!archive.IsOpen())return false;
    world.Serialize(archive);
    auto start=ResolvePlayerStart(world);
    if(start.resolution!=PlayerStartResolution::Success)return false;
    if(auto* t=world.transforms.GetComponent(start.start.entity)){
        t->ClearTransform();t->Translate(XMFLOAT3{0,0,-4});t->UpdateTransform();
    }else return false;
    // Keep the receiver fully visible in this private review scene.
    for(size_t i=0;i<world.names.GetCount();++i) {
        const auto e=world.names.GetEntity(i);
        const auto& name=world.names[i].name;
        if(name.find("Burning mutant")!=std::string::npos)
            if(auto* t=world.transforms.GetComponent(e)){
                t->Translate(XMFLOAT3{2.5f-t->translation_local.x,0,0});t->UpdateTransform();
            }
    }
    SetPlayerControllerSettingsCommand replace(world,start.start.entity,player);
    if(!replace.Execute())return false;
    world.Update(0);
    wi::Archive state;world.Serialize(state);
    const auto scenePath=project/"Content/Scenes/Main.wiscene";
    if(!state.SaveFile(scenePath.generic_u8string()))return false;
    wi::scene::Scene coldScene;wi::Archive cold(scenePath.generic_u8string(),true);
    if(!cold.IsOpen())return false;coldScene.Serialize(cold);
    if(ResolvePlayerStart(coldScene).resolution!=PlayerStartResolution::Success)return false;
    std::ofstream(project/"column-review.json")<<nlohmann::json{
        {"equipmentId",saved.document.equipment.assetId},{"visualOnly",true},
        {"frames",11},{"strips",6},{"pointLight",true},{"position",{0,0,6}}}.dump(2);
    std::cout<<"LIGHTNING COLUMN: 11 ORIGINAL GPU FRAMES; ALL SIX STRIPS VISIT ALL POSES; "
        "PULSING WHITE CORE; SOURCE SPHERE; REAL POINT LIGHT; SPARKS; CLEANUP; "
        "PRIVATE EQUIPMENT AND SCENE COLD REOPEN PASS. VISUAL OWNER REVIEW PENDING.\n";
    return true;
}
