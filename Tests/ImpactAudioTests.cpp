#include "../Runtime/src/RuntimeProjectileVisuals.h"
#include "../Runtime/src/RuntimeBloodEffects.h"
#include "renegade/bridge/CharacterService.h"
#include "../Runtime/src/RuntimeImpactTextures.h"
#include "renegade/bridge/ImpactAudioService.h"
#include "renegade/bridge/ImpactDefaultsService.h"
#include "renegade/bridge/StoryFlowLevelReferenceService.h"
#include <set>
#include <mmdeviceapi.h>
#include <cstdlib>
#include "renegade/bridge/ProjectileAssetService.h"
#include "renegade/bridge/EquipmentAssetService.h"
#include "renegade/bridge/PlayerService.h"
#include "renegade/bridge/BuildStageService.h"
#include "renegade/bridge/BuildIdentityService.h"
#include "renegade/bridge/PackageIntegrityService.h"
#include "renegade/bridge/ObjectImpactSurfaceService.h"
#include "renegade/bridge/SceneDocumentService.h"
#include "renegade/bridge/SceneService.h"
#include "renegade/bridge/SelectionService.h"
#include "renegade/bridge/ProjectService.h"
#include "renegade/bridge/WindowsGameBuildProjectService.h"
#include <filesystem>
#include "renegade/bridge/ReusableAssetService.h"
#include <fstream>
#include <iostream>
#include <iterator>
#include <cmath>
#include "json.hpp"
using namespace renegade::bridge;
namespace fs=std::filesystem;
static std::vector<std::uint8_t> Bytes(const fs::path& p)
{ std::ifstream in(p,std::ios::binary); return {std::istreambuf_iterator<char>(in),{}}; }
static void Put(const fs::path& p,const std::vector<std::uint8_t>& bytes)
{ fs::create_directories(p.parent_path()); std::ofstream out(p,std::ios::binary); out.write((const char*)bytes.data(),bytes.size()); }
static void Text(const fs::path& p,const std::string& text) { Put(p,{text.begin(),text.end()}); }
static int Install(const fs::path& descriptor,const fs::path& first,const fs::path& second)
{
    ProjectMetadata project; std::string error;
    if (!ProjectService().InspectProject(descriptor.generic_u8string(),project,error))
    { std::cerr<<error; return 1; }
    ImpactAudioBank bank; bank.projectId=project.projectId;
    // Explicit supplied-pack mapping. Runtime never infers categories from filenames.
    const char* folders[]={"Metal","Wood","Concrete","Stone","Dirt","Glass","Water"};
    const char* tokens[]={"metal","wood","concrete","rock","dirt","glass","water"};
    for (unsigned i=0;i<7;++i) for (int variant=0;variant<2;++variant)
    {
        const std::string filename=std::string("bullet_hitting_")+
            ((i==3 && variant==1)?"stone":tokens[i])+(variant?"2":"")+".wav";
        fs::path input;
        for (const auto& entry:fs::recursive_directory_iterator(variant?second:first))
            if(entry.is_regular_file() && entry.path().filename()==filename) input=entry.path();
        auto payload=Bytes(input);
        if (!ValidateImpactAudioPayload(payload,error)) { std::cerr<<filename<<": "<<error; return 2; }
        const std::string base=std::string("Audio/Impacts/")+folders[i]+"/Impact_"+std::to_string(variant+1);
        ResourceAssetImportRequest request;
        request.projectRoot=project.rootPath; request.projectId=project.projectId;
        request.sourceProjectRelativePath="SourceAssets/"+base+".wav";
        request.assetProjectRelativePath="Content/"+base+".rasset";
        request.expectedFormat=ResourceSourceFormat::Wav;
        auto source=fs::u8path(project.rootPath)/request.sourceProjectRelativePath;
        if(fs::exists(source) && Bytes(source)!=payload) { std::cerr<<"Source collision: "<<source; return 3; }
        Put(source,payload);
        fs::create_directories(fs::u8path(project.rootPath)/fs::u8path(request.assetProjectRelativePath).parent_path());
        const auto result=ResourceAssetService().ImportResourceAsset(request);
        if(!result.succeeded) { std::cerr<<result.error; return 4; }
        ImpactSurfaceType type; ParseImpactSurfaceType(i==3?"stone":tokens[i],type);
        bank.surfaces[static_cast<unsigned>(type)].push_back(result.assetId);
        std::cout<<folders[i]<<" "<<variant+1<<" "<<result.assetId<<"\n";
    }
    if(!WriteImpactAudioBank(project.rootPath,bank,error)) { std::cerr<<error; return 5; }
    std::cout<<"Imported 14 impact sounds and saved bank"<<std::endl;
    return 0;
}

// Manual read-only proof against the actual authored level; never saves its transient effects.
static int VerifyBloodScene(const fs::path& descriptor,const fs::path& sheetFolder={},bool install=false)
{
    HWND window=CreateWindowExW(0,L"STATIC",L"Blood scene verification",
        WS_OVERLAPPEDWINDOW,0,0,960,640,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    wi::Application app;app.allow_hdr=false;app.SetWindow(window);
    wi::initializer::InitializeComponentsImmediate();
    struct Drain { ~Drain(){while(wi::renderer::IsPipelineCreationActive())Sleep(10);
        wi::graphics::GetDevice()->WaitForGPU();} } drain;
    ProjectService projects;ProjectMetadata project;std::string error;
    if(!projects.InspectProject(descriptor.generic_u8string(),project,error) ||
       !projects.OpenProject(descriptor.generic_u8string()))return 40;
    auto scenes=std::make_unique<SceneService>();SelectionService selection;CommandService commands;
    SceneDocumentService documents(*scenes,selection,commands,projects);
    if(!documents.Open((fs::u8path(project.rootPath)/project.startupScene).generic_u8string()))return 41;
    auto& scene=scenes->GetScene();
    if(install) {
        const auto donor=renegade::runtime::RuntimeBloodSheets::FindMaterial(scene);
        size_t duplicates=0;
        for(size_t i=0;i<scene.metadatas.GetCount();++i)
            if(scene.metadatas.GetEntity(i)==donor)++duplicates;
        if(duplicates>1) {
            // Repair only this disposable fixture's earlier installer duplicates.
            // Wicked Create requires absence; preserve every other entity verbatim.
            std::vector<std::pair<wi::ecs::Entity,wi::scene::MetadataComponent>> retained;
            bool kept=false;
            for(size_t i=0;i<scene.metadatas.GetCount();++i) {
                const auto entity=scene.metadatas.GetEntity(i);
                if(entity==donor && kept)continue;
                retained.emplace_back(entity,scene.metadatas[i]);
                if(entity==donor){kept=true;retained.back().second.bool_values.set(
                    renegade::runtime::RuntimeBloodSheets::MaterialKey,true);}
            }
            scene.metadatas.Clear();
            for(const auto& entry:retained)scene.metadatas.Create(entry.first)=entry.second;
            std::cout<<"Repaired duplicate blood fixture metadata="<<duplicates<<std::endl;
        }
    }
    const auto initialRestore=RestoreMaterialTextureBindings(scene,project.rootPath,project.projectId);
    if(!initialRestore.succeeded){std::cerr<<"Blood scene texture restoration: "<<initialRestore.error;return 51;}
    if(!sheetFolder.empty())
    {
        auto donor=renegade::runtime::RuntimeBloodSheets::FindMaterial(scene);
        if(donor==wi::ecs::INVALID_ENTITY)
        {donor=scene.Entity_CreateTransform("Blood impact material resource");scene.materials.Create(donor);}
        StableId importedIds[2];
        const char* names[]={"KnifeBloodSheet64","KnifeBloodSheetNormal64"};
        for(unsigned i=0;i<2;++i)
        {
            const std::string base=std::string("Textures/Impacts/Blood/")+names[i];
            const auto payload=Bytes(sheetFolder/(std::string(names[i])+".png"));
            ResourceAssetImportRequest request;
            request.projectRoot=project.rootPath;request.projectId=project.projectId;
            request.sourceProjectRelativePath="SourceAssets/"+base+".png";
            request.assetProjectRelativePath="Content/"+base+".rasset";
            request.expectedFormat=ResourceSourceFormat::Png;
            auto product=fs::u8path(project.rootPath)/request.assetProjectRelativePath;
            StableId id;
            if(fs::exists(product))
            {
                ResourceAssetDocument document;
                if(!ReadResourceAssetDocument(product.generic_u8string(),document,error) || document.payload!=payload)
                {std::cerr<<"Blood texture product collision: "<<error;return 46;}
                id=document.manifest.assetId;
            }
            else
            {
                Put(fs::u8path(project.rootPath)/request.sourceProjectRelativePath,payload);
                fs::create_directories(product.parent_path());
                const auto imported=ResourceAssetService().ImportResourceAsset(request);
                if(!imported.succeeded){std::cerr<<imported.error;return 47;}
                id=imported.assetId;
            }
            importedIds[i]=id;
            PreparedMaterialTextureAsset prepared;
            if(!PrepareMaterialTextureAsset(project.rootPath,project.projectId,id,prepared,error) ||
               !ApplyPreparedMaterialTextureAsset(scene,donor,i?MaterialTextureSlot::Normal:MaterialTextureSlot::BaseColor,
                   prepared,{},error)){std::cerr<<error;return 48;}
            std::cout<<"Governed blood texture "<<names[i]<<" "<<id<<std::endl;
        }
        auto& material=*scene.materials.GetComponent(donor);
        material.baseColor={.45f,.008f,.018f,1};material.SetRoughness(.32f);
        material.SetReflectance(.012f);material.SetMetalness(0);
        material.SetNormalMapStrength(.65f);material.SetDoubleSided(true);
        material.userBlendMode=wi::enums::BLENDMODE_ALPHA;material.emissiveColor={0,0,0,0};material.SetDirty();
        auto* metadata=scene.metadatas.GetComponent(donor);
        if(!metadata)metadata=&scene.metadatas.Create(donor);
        metadata->bool_values.set(renegade::runtime::RuntimeBloodSheets::MaterialKey,true);
        if(install)
        {
            const auto path=(fs::u8path(project.rootPath)/project.startupScene).generic_u8string();
            if(!documents.Save(path) || !documents.Reload()){std::cerr<<"Blood preset save/reload failed";return 49;}
            const auto restored=RestoreMaterialTextureBindings(scene,project.rootPath,project.projectId);
            if(!restored.succeeded || renegade::runtime::RuntimeBloodSheets::FindMaterial(scene)==wi::ecs::INVALID_ENTITY)
            {std::cerr<<"Blood preset governed reload failed: "<<restored.error;return 50;}
            const auto current=renegade::runtime::RuntimeBloodSheets::FindMaterial(scene);
            size_t count=0;
            for(size_t i=0;i<scene.metadatas.GetCount();++i)
                if(scene.metadatas.GetEntity(i)==current)++count;
            const auto* metadata=scene.metadatas.GetComponent(current);
            if(count!=1 || !metadata ||
               metadata->string_values.get(MaterialBaseColorTextureAssetIdMetadataKey)!=importedIds[0] ||
               metadata->string_values.get(MaterialNormalTextureAssetIdMetadataKey)!=importedIds[1]) {
                std::cerr<<"Blood preset reload changed governed identity or duplicated metadata";return 52;
            }
            std::cout<<"Native blood preset saved and reloaded"<<std::endl;
        }
    }
    // This read-only rendering diagnostic does not run Runtime's Character
    // initialization/controller. Freeze native Characters at their authored pose.
    for(size_t i=0;i<scene.characters.GetCount();++i)
    {
        if(auto* transform=scene.transforms.GetComponent(scene.characters.GetEntity(i)))
        {transform->UpdateTransform();scene.characters[i].SetPosition(transform->GetPosition());}
        scene.characters[i].gravity=0;scene.characters[i].velocity={0,0,0};
    }
    if(const auto* material=scene.materials.GetComponent(renegade::runtime::RuntimeBloodSheets::FindMaterial(scene)))
        std::cout<<"Blood material color="<<material->baseColor.x<<","<<material->baseColor.y<<","<<material->baseColor.z
            <<" roughness="<<material->roughness<<" reflectance="<<material->reflectance<<std::endl;
    if(const auto* m=scene.materials.GetComponent(renegade::runtime::RuntimeBloodSheets::FindMaterial(scene))) {
        const auto& t=m->textures[wi::scene::MaterialComponent::BASECOLORMAP];
        std::cout<<"Blood atlas resource="<<t.name<<" uvset="<<t.uvset<<std::endl;
        wi::vector<uint8_t> data;
        if(wi::helper::saveTextureToMemoryFile(t.resource.GetTexture(),"PNG",data)) {
            std::ofstream file("BUILD/p3-blood-loaded-atlas.png",std::ios::binary);
            file.write(reinterpret_cast<const char*>(data.data()),data.size());
        }
    }
    scene.Update(.016f);
    for(size_t i=0;i<scene.objects.GetCount();++i)
    {
        const auto entity=scene.objects.GetEntity(i);const auto* name=scene.names.GetComponent(entity);
        if(name && name->name.find("Blood dummy")!=std::string::npos)
        {
            const auto position=scene.transforms.GetComponent(entity)->GetPosition();
            std::cout<<name->name<<" world="<<position.x<<","<<position.y<<","<<position.z
                <<" frontZ="<<scene.aabb_objects[i].getMin().z<<std::endl;
        }
    }
    auto hits=wi::vector<wi::scene::Scene::RayIntersectionResult>{};
    scene.IntersectsAll(hits,wi::primitive::Ray(DirectX::XMFLOAT3{0,1.2f,4.6f},DirectX::XMFLOAT3{0,-1,0},0,4),
        wi::enums::FILTER_OPAQUE|wi::enums::FILTER_TRANSPARENT|wi::enums::FILTER_WATER|wi::enums::FILTER_TERRAIN);
    for(const auto& hit:hits) {
        const auto* name=scene.names.GetComponent(hit.entity);
        std::cout<<"Downward receiver: "<<(name?name->name:"unnamed")<<" distance="<<hit.distance
            <<" y="<<hit.position.y<<std::endl;
        const auto scale=scene.transforms.GetComponent(hit.entity)->GetScale();
        std::cout<<"Receiver world scale="<<scale.x<<","<<scale.y<<","<<scale.z<<std::endl;
    }
    renegade::runtime::RuntimeBloodEffects blood;
    blood.Spawn(scene,{0,1.2f,5.22f},{0,0,-1},wi::ecs::INVALID_ENTITY,.55f,17);
    for(unsigned frame=0;frame<110;++frame){scene.Update(.016f);blood.Update(scene,.016f,{});}
    std::cout<<"Actual playground blood stains="<<blood.stains.size()<<" drops="<<blood.drops.size()<<std::endl;
    for(const auto& stain:blood.stains) {
        const auto* hierarchy=scene.hierarchy.GetComponent(stain.entity);
        const auto* name=hierarchy?scene.names.GetComponent(hierarchy->parentID):nullptr;
        const auto position=scene.transforms.GetComponent(stain.entity)->GetPosition();
        std::cout<<"Stain receiver="<<(name?name->name:"none")<<" position="
            <<position.x<<","<<position.y<<","<<position.z<<std::endl;
    }
    wi::renderer::SetTemporalAAEnabled(false);
    wi::RenderPath3D render;wi::scene::CameraComponent camera;
    render.scene=&scene;render.camera=&camera;render.init(960,640,96);
    render.setDepthOfFieldEnabled(false);render.setMotionBlurEnabled(false);
    render.ResizeBuffers();
    camera.CreatePerspective(960,640,.01f,100.f,DirectX::XMConvertToRadians(65.f));
    camera.TransformCamera(DirectX::XMMatrixInverse(nullptr,DirectX::XMMatrixLookAtLH(
        DirectX::XMVectorSet(0,2.4f,2.6f,1),DirectX::XMVectorSet(0,0,4.6f,1),DirectX::XMVectorSet(0,1,0,0))));
    camera.UpdateCamera();
    for(unsigned frame=0;frame<60;++frame) {
        wi::eventhandler::FireEvent(wi::eventhandler::EVENT_THREAD_SAFE_POINT,0);
        render.PreUpdate();render.Update(.016f);render.PreRender();render.Render();
        wi::graphics::GetDevice()->SubmitCommandLists();wi::renderer::UpdateGPUSuballocator();Sleep(10);
    }
    wi::vector<uint8_t> png;
    if(!wi::helper::saveTextureToMemoryFile(render.GetRenderResult3D(),"PNG",png))return 42;
    std::ofstream output("BUILD/p3-blood-actual-floor.png",std::ios::binary);
    output.write(reinterpret_cast<const char*>(png.data()),png.size());output.close();
    const bool passed=!blood.stains.empty();blood.Reset(scene);
    wi::vector<uint8_t> sourcePng;
    if(wi::helper::saveTextureToMemoryFile(renegade::runtime::GetBloodSplatTexture(),"PNG",sourcePng)) {
        std::ofstream source("BUILD/p3-blood-native-splat.png",std::ios::binary);
        source.write(reinterpret_cast<const char*>(sourcePng.data()),sourcePng.size());
    }
    camera.TransformCamera(DirectX::XMMatrixInverse(nullptr,DirectX::XMMatrixLookAtLH(
        DirectX::XMVectorSet(0,1.5f,2.8f,1),DirectX::XMVectorSet(0,1.2f,5.3f,1),DirectX::XMVectorSet(0,1,0,0))));
    camera.UpdateCamera();
    renegade::runtime::RuntimeProjectileVisuals visuals;
    renegade::bridge::ProjectileImpact impact;
    impact.contact.position={0,1.2f,5.22f};impact.contact.normal={0,0,-1};
    impact.contact.surfaceType=ImpactSurfaceType::Character;impact.incomingVelocity={0,0,10};
    if(const char* direction=std::getenv("RENEGADE_BLOOD_DIRECTION"))
    {
        if(std::string(direction)=="side"){impact.contact.normal={-1,0,0};impact.incomingVelocity={10,0,0};}
        if(std::string(direction)=="angled"){impact.contact.normal={0,0,-1};impact.incomingVelocity={7,0,7};}
        if(std::string(direction)=="upper"){impact.contact.position={0,1.75f,5.35f};impact.contact.normal={0,.8f,-.6f};}
        if(std::string(direction)=="lower"){impact.contact.position={0,.8f,5.3f};impact.contact.normal={0,-.8f,-.6f};}
    }
    wi::ecs::Entity source=wi::ecs::INVALID_ENTITY;
    for(size_t i=0;i<scene.names.GetCount();++i)
        if(scene.names[i].name=="Blood response Character dummy")source=scene.names.GetEntity(i);
    if(std::getenv("RENEGADE_BLOOD_HIDE_DUMMY"))
        for(size_t i=0;i<scene.objects.GetCount();++i) {
            const auto* name=scene.names.GetComponent(scene.objects.GetEntity(i));
            if(name && name->name.find("Blood dummy")!=std::string::npos)
                scene.objects[i].SetRenderable(false);
        }
    if(std::getenv("RENEGADE_ARROW_POSE_PROOF")) {
        std::string error;
        const auto definitions=ListProjectileAssets(project.rootPath,project.projectId,error);
        auto found=std::find_if(definitions.begin(),definitions.end(),[](const auto& d) {
            return !d.meshAssetId.empty() && d.name=="Flaming Arrow - Stick";
        });
        if(found==definitions.end()){std::cerr<<"No governed Arrow for pose proof";return 53;}
        auto definition=*found;definition.stickOnImpact=true;definition.stuckLifetimeSeconds=10;
        definition.impactEffect=renegade::bridge::ProjectileEffectKind::None;
        definition.flightEffects.clear();
        if(!visuals.Prepare(project.rootPath,"",project.projectId,definition)) {
            std::cerr<<visuals.error;return 54;
        }
        const auto& arrowModel=*visuals.templates.at(definition.assetId).scene;
        for(size_t i=0;i<arrowModel.objects.GetCount();++i) {
            const auto* mesh=arrowModel.meshes.GetComponent(arrowModel.objects[i].meshID);
            const auto* transform=arrowModel.transforms.GetComponent(arrowModel.objects.GetEntity(i));
            XMFLOAT3 lo={10000,10000,10000},hi={-10000,-10000,-10000};
            for(const auto& v:mesh->vertex_positions) {
                XMFLOAT3 p;XMStoreFloat3(&p,XMVector3TransformCoord(XMLoadFloat3(&v),transform->GetWorldMatrix()));
                lo.x=std::min(lo.x,p.x);lo.y=std::min(lo.y,p.y);lo.z=std::min(lo.z,p.z);
                hi.x=std::max(hi.x,p.x);hi.y=std::max(hi.y,p.y);hi.z=std::max(hi.z,p.z);
            }
            std::cout<<"Arrow template bounds "<<lo.x<<","<<lo.y<<","<<lo.z<<" / "<<hi.x<<","<<hi.y<<","<<hi.z<<std::endl;
        }
        const auto wall=scene.Entity_CreateCube("Angled arrow scaled receiver");
        scene.materials.GetComponent(wall)->baseColor={.4f,.4f,.4f,1};
        auto* t=scene.transforms.GetComponent(wall);
        t->Scale(XMFLOAT3{2,.5f,.025f});t->Translate(XMFLOAT3{0,1.2f,5.5f});t->UpdateTransform();
        const renegade::bridge::ProjectileVector velocities[]={{0,0,10},{6,0,8},{9.5f,0,3.1f}};
        for(unsigned i=0;i<3;++i) {
            if(!visuals.Spawn(scene,definition.assetId,100+i,true))return 55;
            auto arrowImpact=impact;arrowImpact.projectileId=100+i;
            arrowImpact.contact.position={float(i)-1,1.2f,5.45f};
            arrowImpact.contact.surfaceType=ImpactSurfaceType::Concrete;
            arrowImpact.incomingVelocity=velocities[i];visuals.Impact(scene,arrowImpact,wall);
        }
        std::cout<<"Governed arrow native proof front / oblique / grazing; nonuniform receiver"<<std::endl;
    } else visuals.PresentSurfaceImpact(scene,impact,source);
    if(std::getenv("RENEGADE_BLOOD_DROPS_ONLY"))visuals.blood.liquidSheets.Reset(scene);
    if(std::getenv("RENEGADE_BLOOD_SHEETS_ONLY")) {
        for(const auto& drop:visuals.blood.drops)scene.Entity_Remove(drop.entity);
        visuals.blood.drops.clear();
    }
    if(std::getenv("RENEGADE_BLOOD_CLOSE_CAMERA")) {
        camera.TransformCamera(DirectX::XMMatrixInverse(nullptr,DirectX::XMMatrixLookAtLH(
            DirectX::XMVectorSet(0,1.2f,4.6f,1),DirectX::XMVectorSet(0,1.2f,5.3f,1),DirectX::XMVectorSet(0,1,0,0))));
        camera.UpdateCamera();
    }
    renegade::bridge::ProjectileSimulation simulation;
    for(unsigned frame=0;frame<110;++frame) {
        const bool lateSync=std::getenv("RENEGADE_BLOOD_LATE_SYNC")!=nullptr;
        if(!lateSync)visuals.Sync(scene,simulation,.016f);
        wi::eventhandler::FireEvent(wi::eventhandler::EVENT_THREAD_SAFE_POINT,0);
        render.PreUpdate();render.Update(.016f);
        if(lateSync)visuals.Sync(scene,simulation,.016f);
        render.PreRender();render.Render();
        wi::graphics::GetDevice()->SubmitCommandLists();wi::renderer::UpdateGPUSuballocator();Sleep(10);
        if(frame==20)
        {
            for(const auto& sheet:visuals.blood.liquidSheets.sheets)
            {
                const auto p=scene.transforms.GetComponent(sheet.entity)->GetPosition();
                const auto index=scene.objects.GetIndex(sheet.entity);
                std::cout<<"Liquid sheet world="<<p.x<<","<<p.y<<","<<p.z
                    <<" frontZ="<<scene.aabb_objects[index].getMin().z
                    <<" backZ="<<scene.aabb_objects[index].getMax().z<<std::endl;
            }
            for(size_t i=0;i<scene.objects.GetCount();++i)
            {
                const auto* name=scene.names.GetComponent(scene.objects.GetEntity(i));
                if(name && name->name.find("Blood dummy")!=std::string::npos)
                    std::cout<<name->name<<" frame20 frontZ="<<scene.aabb_objects[i].getMin().z
                        <<" foreground="<<scene.objects[i].IsForeground()
                        <<" blend="<<scene.materials.GetComponent(scene.meshes.GetComponent(scene.objects[i].meshID)->subsets[0].materialID)->userBlendMode<<std::endl;
            }
        }
        if(frame==5 || frame==12 || frame==20 || frame==23 || frame==24 || frame==29 || frame==30 || frame==40 || frame==60 || frame==90) {
            wi::vector<uint8_t> framePng;
            if(!wi::helper::saveTextureToMemoryFile(render.GetRenderResult3D(),"PNG",framePng))return 44;
            std::ofstream shot("BUILD/p3-blood-atlas-frame-"+std::to_string(frame)+".png",std::ios::binary);
            shot.write(reinterpret_cast<const char*>(framePng.data()),framePng.size());
        }
    }
    visuals.Reset(scene);
    std::cout<<"Native blood sequence rendered at five different ages"<<std::endl;
    return passed?0:43;
}
// Native visual proof on a real horizontal water shader; optional disposable fixture install.
static int VerifyMarksScene(const fs::path& descriptor,const fs::path& folder={},bool install=false)
{
    HWND window=CreateWindowExW(0,L"STATIC",L"Impact mark verification",WS_OVERLAPPEDWINDOW,
        0,0,960,640,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    wi::Application app;app.allow_hdr=false;app.SetWindow(window);
    wi::initializer::InitializeComponentsImmediate();
    struct Drain { ~Drain(){while(wi::renderer::IsPipelineCreationActive())Sleep(10);
        wi::graphics::GetDevice()->WaitForGPU();} } drain;
    ProjectService projects;ProjectMetadata project;std::string error;
    if(!projects.InspectProject(descriptor.generic_u8string(),project,error) ||
        !projects.OpenProject(descriptor.generic_u8string()))return 100;
    auto scenes=std::make_unique<SceneService>();SelectionService selection;CommandService commands;
    SceneDocumentService documents(*scenes,selection,commands,projects);
    const auto scenePath=(fs::u8path(project.rootPath)/project.startupScene).generic_u8string();
    if(!documents.Open(scenePath))return 101;
    auto& scene=scenes->GetScene();
    if(!RestoreMaterialTextureBindings(scene,project.rootPath,project.projectId).succeeded)return 102;
    using Marks=renegade::runtime::RuntimeImpactMarks;
    const ImpactSurfaceType surfaces[]={ImpactSurfaceType::Metal,ImpactSurfaceType::Wood,
        ImpactSurfaceType::Concrete,ImpactSurfaceType::Stone,ImpactSurfaceType::Glass,
        ImpactSurfaceType::Dirt,ImpactSurfaceType::Character};
    const char* names[]={"Metal","Wood","Concrete","Stone","Glass","Dirt","Character"};
    const char* slots[]={"Base","Normal","Surface"};
    const MaterialTextureSlot slotTypes[]={MaterialTextureSlot::BaseColor,MaterialTextureSlot::Normal,MaterialTextureSlot::Surface};
    StableId ids[7][3];
    if(install)for(unsigned i=0;i<7;++i) {
        auto donor=Marks::FindMaterial(scene,surfaces[i]);
        if(donor==wi::ecs::INVALID_ENTITY) {
            donor=scene.Entity_CreateTransform(std::string("Knife ")+names[i]+" decal resource");scene.materials.Create(donor);
        }
        for(unsigned j=0;j<3;++j) {
            const std::string name=std::string(names[i])+slots[j];
            const auto payload=Bytes(folder/(name+".png"));
            const std::string base="Textures/Impacts/Marks/Knife"+name;
            ResourceAssetImportRequest request;request.projectRoot=project.rootPath;request.projectId=project.projectId;
            request.sourceProjectRelativePath="SourceAssets/"+base+".png";
            request.assetProjectRelativePath="Content/"+base+".rasset";request.expectedFormat=ResourceSourceFormat::Png;
            const auto product=fs::u8path(project.rootPath)/request.assetProjectRelativePath;
            if(fs::exists(product)) {
                ResourceAssetDocument document;
                if(!ReadResourceAssetDocument(product.generic_u8string(),document,error) || document.payload!=payload)return 103;
                ids[i][j]=document.manifest.assetId;
            } else {
                Put(fs::u8path(project.rootPath)/request.sourceProjectRelativePath,payload);fs::create_directories(product.parent_path());
                const auto imported=ResourceAssetService().ImportResourceAsset(request);
                if(!imported.succeeded){std::cerr<<imported.error;return 104;}ids[i][j]=imported.assetId;
            }
            PreparedMaterialTextureAsset prepared;
            if(!PrepareMaterialTextureAsset(project.rootPath,project.projectId,ids[i][j],prepared,error) ||
                !ApplyPreparedMaterialTextureAsset(scene,donor,slotTypes[j],prepared,{},error))
                {std::cerr<<error;return 105;}
        }
        auto& m=*scene.materials.GetComponent(donor);m.baseColor={1,1,1,1};
        m.userBlendMode=wi::enums::BLENDMODE_ALPHA;m.SetRoughness(1);m.SetReflectance(1);
        m.SetMetalness(0);m.SetNormalMapStrength(.65f);m.emissiveColor={0,0,0,0};m.SetDirty();
        auto* metadata=scene.metadatas.GetComponent(donor);
        if(!metadata)metadata=&scene.metadatas.Create(donor);
        metadata->bool_values.set(Marks::Key(surfaces[i]),true);
    }
    if(install && (!documents.Save(scenePath) || !documents.Reload()))return 106;
    if(!RestoreMaterialTextureBindings(scene,project.rootPath,project.projectId).succeeded)return 107;
    for(unsigned i=0;i<7;++i) {
        const auto donor=Marks::FindMaterial(scene,surfaces[i]);if(donor==wi::ecs::INVALID_ENTITY)return 108;
        for(unsigned j=0;j<3;++j) {
            const auto* m=scene.materials.GetComponent(donor);
            if(!m->textures[WickedTextureSlot(slotTypes[j])].resource.GetTexture().IsValid())return 109;
            if(install && scene.metadatas.GetComponent(donor)->string_values.get(
                MaterialTextureSlotMetadataKey(slotTypes[j]))!=ids[i][j])return 110;
        }
        std::cout<<names[i]<<" governed colour/normal/surface atlas reload PASS"<<std::endl;
    }
    for(size_t i=0;i<scene.characters.GetCount();++i)scene.characters[i].SetActive(false);
    for(size_t i=0;i<scene.objects.GetCount();++i)scene.objects[i].SetRenderable(false);
    const auto receiver=scene.Entity_CreateCube("Read-only decal proof receiver");
    auto* rt=scene.transforms.GetComponent(receiver);rt->Translate(XMFLOAT3{0,1.2f,3.3f});rt->Scale(XMFLOAT3{.4f,.8f,.2f});rt->UpdateTransform();
    auto* rm=scene.materials.GetComponent(receiver);rm->baseColor={.48f,.42f,.35f,1};rm->SetRoughness(.7f);
    wi::renderer::SetTemporalAAEnabled(false);
    wi::RenderPath3D render;wi::scene::CameraComponent camera;render.scene=&scene;render.camera=&camera;render.init(960,640,96);
    render.setDepthOfFieldEnabled(false);render.setMotionBlurEnabled(false);render.ResizeBuffers();
    camera.CreatePerspective(960,640,.01f,100.f,DirectX::XMConvertToRadians(55.f));
    camera.TransformCamera(DirectX::XMMatrixInverse(nullptr,DirectX::XMMatrixLookAtLH(
        DirectX::XMVectorSet(0,1.2f,2.5f,1),DirectX::XMVectorSet(0,1.2f,3.1f,1),DirectX::XMVectorSet(0,1,0,0))));camera.UpdateCamera();
    const auto frame=[&]() {wi::eventhandler::FireEvent(wi::eventhandler::EVENT_THREAD_SAFE_POINT,0);
        render.PreUpdate();render.Update(.016f);render.PreRender();render.Render();
        wi::graphics::GetDevice()->SubmitCommandLists();wi::renderer::UpdateGPUSuballocator();Sleep(10);};
    for(unsigned i=0;i<20;++i)frame();
    for(unsigned i=0;i<7;++i) {
        renegade::runtime::RuntimeProjectileVisuals visuals;renegade::bridge::ProjectileSimulation simulation;
        renegade::bridge::ProjectileImpact impact;impact.projectileId=17;impact.contact.position={0,1.2f,3.1f};
        impact.contact.normal={0,0,-1};impact.contact.surfaceType=surfaces[i];impact.incomingVelocity={0,0,10};
        if(i==6) {
            visuals.instanceAssets[17]="wound-trickle-proof";
            visuals.templates["wound-trickle-proof"].definition.stickOnImpact=true;
        }
        visuals.PresentSurfaceImpact(scene,impact,receiver);
        if(visuals.impactDecals.size()!=((i==4 || i==6)?2u:1u))return 111;
        if(i==6) {
            if(visuals.impactDecals.back().bleedAge!=0)return 119;
            visuals.Sync(scene,simulation,0);
            if(visuals.impactDecals.back().bleedAge!=0)return 120;
        }
        const auto entity=visuals.impactDecals.front().entity;
        if(i==4 && std::getenv("RENEGADE_GLASS_COLOR_ONLY")) {
            auto* m=scene.materials.GetComponent(entity);
            m->textures[wi::scene::MaterialComponent::NORMALMAP]={};
            m->textures[wi::scene::MaterialComponent::SURFACEMAP]={};m->SetDirty();
        }
        const auto& markTransform=*scene.transforms.GetComponent(entity);
        const auto markMatrix=XMLoadFloat4x4(&markTransform.world);
        const float markWidth=XMVectorGetX(XMVector3Length(markMatrix.r[0]));
        const float markHeight=XMVectorGetX(XMVector3Length(markMatrix.r[1]));
        if(std::abs(markWidth-markHeight)>.00001f || scene.hierarchy.Contains(entity))return 117;
        for(unsigned n=0;n<100;++n){visuals.Sync(scene,simulation,.016f);frame();}
        wi::vector<uint8_t> png;if(!wi::helper::saveTextureToMemoryFile(render.GetRenderResult3D(),"PNG",png))return 112;
        std::ofstream out("BUILD/p3-knife-mark-"+std::string(names[i])+(std::getenv("RENEGADE_GLASS_COLOR_ONLY")?"-colour-only":"-full")+".png",std::ios::binary);
        out.write(reinterpret_cast<const char*>(png.data()),png.size());out.close();
        if(i==0) {
            for(const float distance:{1.2f,2.5f,4.f}) {
                camera.TransformCamera(DirectX::XMMatrixInverse(nullptr,DirectX::XMMatrixLookAtLH(
                    DirectX::XMVectorSet(0,1.2f,3.1f-distance,1),DirectX::XMVectorSet(0,1.2f,3.1f,1),DirectX::XMVectorSet(0,1,0,0))));
                camera.UpdateCamera();for(unsigned f=0;f<12;++f){visuals.Sync(scene,simulation,.016f);frame();}
                wi::vector<uint8_t> farPng;
                if(!wi::helper::saveTextureToMemoryFile(render.GetRenderResult3D(),"PNG",farPng))return 118;
                std::ofstream farOut("BUILD/p3-mark-distance-"+std::to_string(int(distance*10))+".png",std::ios::binary);
                farOut.write(reinterpret_cast<const char*>(farPng.data()),farPng.size());
            }
            camera.TransformCamera(DirectX::XMMatrixInverse(nullptr,DirectX::XMMatrixLookAtLH(
                DirectX::XMVectorSet(0,1.2f,2.5f,1),DirectX::XMVectorSet(0,1.2f,3.1f,1),DirectX::XMVectorSet(0,1,0,0))));camera.UpdateCamera();
        }
        if(i==6) {
            const float midHeight=visuals.impactDecals.back().anchor.height;
            if(midHeight<=.002f || midHeight>=.06f)return 121;
            for(unsigned n=0;n<175;++n){visuals.Sync(scene,simulation,.016f);frame();}
            const auto& trail=visuals.impactDecals.back();
            if(std::abs(trail.anchor.height-.06f)>.00001f || trail.anchor.gravityAligned)return 122;
            const auto age=trail.bleedAge;visuals.Sync(scene,simulation,0);
            if(visuals.impactDecals.back().bleedAge!=age)return 123;
            wi::vector<uint8_t> latePng;
            if(!wi::helper::saveTextureToMemoryFile(render.GetRenderResult3D(),"PNG",latePng))return 124;
            std::ofstream lateOut("BUILD/p3-wound-trickle-grown.png",std::ios::binary);
            lateOut.write(reinterpret_cast<const char*>(latePng.data()),latePng.size());lateOut.close();
            std::cout<<"Arrow wound trickle gradual growth, pause and stop PASS"<<std::endl;
            const auto anchor=visuals.impactDecals.front().anchor;if(anchor.receiver!=receiver)return 113;
            rt=scene.transforms.GetComponent(receiver);rt->Translate(XMFLOAT3{.12f,.03f,0});rt->UpdateTransform();
            scene.Update(0);visuals.Sync(scene,simulation,0);
            const auto expected=scene.GetPositionOnSurface(receiver,anchor.a,anchor.b,anchor.c,anchor.bary);
            const auto actual=scene.transforms.GetComponent(entity)->GetPosition();
            if(XMVectorGetX(XMVector3Length(XMLoadFloat3(&expected)-XMLoadFloat3(&actual)))>.003f)return 114;
            auto* mesh=scene.meshes.GetComponent(anchor.mesh);mesh->vertex_positions[anchor.a].x+=.08f;
            scene.Update(0);visuals.Sync(scene,simulation,0);
            const auto deformed=scene.GetPositionOnSurface(receiver,anchor.a,anchor.b,anchor.c,anchor.bary);
            const auto followed=scene.transforms.GetComponent(entity)->GetPosition();
            if(XMVectorGetX(XMVector3Length(XMLoadFloat3(&deformed)-XMLoadFloat3(&followed)))>.003f)return 115;
            std::cout<<"Skin anchor follows receiver movement and triangle deformation PASS"<<std::endl;
        }
        visuals.Sync(scene,simulation,121);if(!visuals.impactDecals.empty())return 116;visuals.Reset(scene);
    }
    std::cout<<"Seven native atlas marks, persistence and expiry PASS"<<std::endl;return 0;
}



static int InstallPlaygroundMutant(const fs::path& descriptor,const fs::path& sourceDescriptor,const std::string& assetId)
{
    HWND window=CreateWindowExW(0,L"STATIC",L"Playground mutant installation",WS_OVERLAPPEDWINDOW,
        0,0,960,640,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    wi::Application app;app.allow_hdr=false;app.SetWindow(window);wi::initializer::InitializeComponentsImmediate();
    struct Drain { ~Drain(){while(wi::renderer::IsPipelineCreationActive())Sleep(10);wi::graphics::GetDevice()->WaitForGPU();} } drain;
    ProjectService projects;ProjectMetadata project,source;std::string error;
    if(!projects.InspectProject(descriptor.generic_u8string(),project,error) ||
        !projects.InspectProject(sourceDescriptor.generic_u8string(),source,error) ||
        !projects.OpenProject(descriptor.generic_u8string()))return 150;
    auto scenes=std::make_unique<SceneService>();SelectionService selection;CommandService commands;
    SceneDocumentService documents(*scenes,selection,commands,projects);
    const auto path=(fs::u8path(project.rootPath)/project.startupScene).generic_u8string();
    if(!documents.Open(path))return 151;
    auto& scene=scenes->GetScene();
    for(size_t i=0;i<scene.names.GetCount();++i)
        if(scene.names[i].name=="Animated Mutant impact target") {
            const auto target=scene.names.GetEntity(i);
            for(size_t o=0;o<scene.objects.GetCount();++o)if(scene.Entity_IsDescendant(scene.objects.GetEntity(o),target)) {
                const auto* mesh=scene.meshes.GetComponent(scene.objects[o].meshID);
                if(mesh)for(const auto& subset:mesh->subsets)if(auto* material=scene.materials.GetComponent(subset.materialID))
                    for(auto& texture:material->textures)if(!texture.resource.IsValid())texture={};
            }
            if(!documents.Save(path) || !documents.Reload())return 159;
            std::cout<<"Existing animated Mutant retained; missing source texture paths cleared and Save/Reload PASS"<<std::endl;return 0;
        }
    ReusableModelPlacementRequest request;request.projectRoot=source.rootPath;request.projectId=source.projectId;request.assetId=assetId;
    auto prepared=ReusableAssetService().PrepareModelAssetPlacement(request);
    if(!prepared.IsReady()){std::cerr<<prepared.Result().error;return 152;}
    std::cout<<"INSTALL prepared"<<std::endl;
    auto* rig=prepared.PeekMutableScene();
    const auto restored=RestoreMaterialTextureBindings(*rig,source.rootPath,source.projectId);
    if(!restored.succeeded){std::cerr<<restored.error;return 153;}
    std::cout<<"INSTALL restored"<<std::endl;
    unsigned textures=0;
    for(size_t i=0;i<rig->materials.GetCount();++i) {
        const auto entity=rig->materials.GetEntity(i);
        for(const auto slot:{MaterialTextureSlot::BaseColor,MaterialTextureSlot::Normal,MaterialTextureSlot::Surface,
            MaterialTextureSlot::Emissive,MaterialTextureSlot::Occlusion}) {
            auto* m=rig->materials.GetComponent(entity);const auto wiSlot=WickedTextureSlot(slot);
            if(!m->textures[wiSlot].resource.IsValid() || !m->textures[wiSlot].resource.GetTexture().IsValid()) {
                if(auto* metadata=rig->metadatas.GetComponent(entity))metadata->string_values.erase(MaterialTextureSlotMetadataKey(slot));
                continue;
            }
            std::vector<uint8_t> payload;ResourceSourceFormat format=ResourceSourceFormat::Unknown;
            const auto* metadata=rig->metadatas.GetComponent(entity);
            if(metadata && metadata->string_values.has(MaterialTextureSlotMetadataKey(slot))) {
                PreparedMaterialTextureAsset original;
                if(!PrepareMaterialTextureAsset(source.rootPath,source.projectId,
                    metadata->string_values.get(MaterialTextureSlotMetadataKey(slot)),original,error))return 154;
                payload=original.payload;format=original.sourceFormat;
            } else {
                const auto& bytes=m->textures[wiSlot].resource.GetFileData();
                payload.assign(bytes.begin(),bytes.end());format=DetectResourceSourceFormat(m->textures[wiSlot].name);
            }
            if(payload.empty() || format==ResourceSourceFormat::Unknown){std::cerr<<"Missing retained texture payload "<<m->textures[wiSlot].name;return 154;}
            std::string extension;
            for(const auto& capability:GetSupportedResourceFormats())if(capability.format==format)extension=capability.extension;
            if(extension.empty())return 154;
            if(extension.front()!='.')extension="."+extension;
            const std::string stem="Textures/MutantImpactTarget/Material"+std::to_string(i)+"Slot"+std::to_string(unsigned(slot));
            ResourceAssetImportRequest import;import.projectRoot=project.rootPath;import.projectId=project.projectId;
            import.sourceProjectRelativePath="SourceAssets/"+stem+extension;import.assetProjectRelativePath="Content/"+stem+".rasset";
            import.expectedFormat=format;Put(fs::u8path(project.rootPath)/import.sourceProjectRelativePath,payload);
            fs::create_directories((fs::u8path(project.rootPath)/import.assetProjectRelativePath).parent_path());
            const auto imported=ResourceAssetService().ImportResourceAsset(import);if(!imported.succeeded){std::cerr<<imported.error;return 155;}
            PreparedMaterialTextureAsset texture;
            if(!PrepareMaterialTextureAsset(project.rootPath,project.projectId,imported.assetId,texture,error) ||
                !ApplyPreparedMaterialTextureAsset(*rig,entity,slot,texture,{},error)){std::cerr<<error;return 156;}
            ++textures;
        }
    }
    std::cout<<"INSTALL textures adopted"<<std::endl;
    wi::vector<wi::ecs::Entity> roots;
    for(size_t i=0;i<rig->transforms.GetCount();++i) {
        const auto entity=rig->transforms.GetEntity(i);if(!rig->hierarchy.Contains(entity))roots.push_back(entity);
    }
    wi::ecs::Entity clip=wi::ecs::INVALID_ENTITY;
    for(size_t i=0;i<rig->animations.GetCount();++i) {
        const auto entity=rig->animations.GetEntity(i);const auto* name=rig->names.GetComponent(entity);
        rig->animations[i].Stop();
        if(clip==wi::ecs::INVALID_ENTITY && name && name->name.find("flexing")!=std::string::npos)clip=entity;
    }
    if(clip==wi::ecs::INVALID_ENTITY)return 157;
    auto* animation=rig->animations.GetComponent(clip);animation->amount=1;animation->SetLooped();animation->RootMotionOff();animation->Play();
    std::cout<<"INSTALL merging"<<std::endl;
    scene.Merge(*rig);
    const auto target=scene.Entity_CreateTransform("Animated Mutant impact target");
    for(const auto entity:roots)scene.Component_Attach(entity,target,true);
    auto* transform=scene.transforms.GetComponent(target);transform->Translate(XMFLOAT3{2,0,5.5f});transform->UpdateTransform();
    CharacterAuthoringSettings settings;settings.role=CharacterRole::Passive;settings.autonomous=false;
    settings.canFlee=false;settings.canUseCover=false;
    std::cout<<"INSTALL promoting"<<std::endl;
    MakeCharacterCommand promote(scene,target,settings);if(!promote.Execute())return 158;
    std::cout<<"INSTALL updating"<<std::endl;
    scene.Update(0);
    std::cout<<"INSTALL saving"<<std::endl;
    if(!documents.Save(path) || !documents.Reload())return 159;
    if(!RestoreMaterialTextureBindings(scene,project.rootPath,project.projectId).succeeded)return 160;
    bool character=false,playing=false;
    for(size_t i=0;i<scene.names.GetCount();++i)if(scene.names[i].name=="Animated Mutant impact target")
        character=IsRenegadeCharacter(scene,scene.names.GetEntity(i));
    for(size_t i=0;i<scene.animations.GetCount();++i) {
        const auto* n=scene.names.GetComponent(scene.animations.GetEntity(i));
        if(n && n->name.find("flexing")!=std::string::npos && scene.animations[i].IsPlaying())playing=true;
    }
    if(!character || !playing)return 161;
    std::cout<<"Animated passive Mutant installed at 2,0,5.5; flexing loop and Character identity Save/Reload PASS; governed textures "<<textures<<std::endl;
    return 0;
}

static int VerifyAnimatedMarks(const fs::path& descriptor,const fs::path& characterProject,const std::string& assetId)
{
    HWND window=CreateWindowExW(0,L"STATIC",L"Animated wound verification",WS_OVERLAPPEDWINDOW,
        0,0,960,640,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    wi::Application app;app.allow_hdr=false;app.SetWindow(window);
    wi::initializer::InitializeComponentsImmediate();
    struct Drain { ~Drain(){while(wi::renderer::IsPipelineCreationActive())Sleep(10);
        wi::graphics::GetDevice()->WaitForGPU();} } drain;
    ProjectService projects;ProjectMetadata project,source;std::string error;
    if(!projects.InspectProject(descriptor.generic_u8string(),project,error) ||
        !projects.InspectProject(characterProject.generic_u8string(),source,error) ||
        !projects.OpenProject(descriptor.generic_u8string()))return 130;
    auto scenes=std::make_unique<SceneService>();SelectionService selection;CommandService commands;
    SceneDocumentService documents(*scenes,selection,commands,projects);
    if(!documents.Open((fs::u8path(project.rootPath)/project.startupScene).generic_u8string()))return 131;
    auto& scene=scenes->GetScene();
    if(!RestoreMaterialTextureBindings(scene,project.rootPath,project.projectId).succeeded)return 132;
    for(size_t i=0;i<scene.objects.GetCount();++i)scene.objects[i].SetRenderable(false);
    for(size_t i=0;i<scene.characters.GetCount();++i)scene.characters[i].SetActive(false);
    ReusableModelPlacementRequest request;request.projectRoot=source.rootPath;request.projectId=source.projectId;request.assetId=assetId;
    auto prepared=ReusableAssetService().PrepareModelAssetPlacement(request);
    if(!prepared.IsReady()){std::cerr<<prepared.Result().error;return 133;}
    auto* rig=prepared.PeekMutableScene();
    wi::vector<wi::ecs::Entity> receivers;
    for(size_t i=0;i<rig->objects.GetCount();++i) {
        const auto* mesh=rig->meshes.GetComponent(rig->objects[i].meshID);
        if(mesh && mesh->IsSkinned())receivers.push_back(rig->objects.GetEntity(i));
    }
    if(receivers.empty())return 134;
    for(size_t i=0;i<rig->characters.GetCount();++i)rig->characters[i].SetActive(false);
    wi::vector<wi::ecs::Entity> rigClips;
    for(size_t i=0;i<rig->animations.GetCount();++i)rigClips.push_back(rig->animations.GetEntity(i));
    scene.Merge(*rig);
    wi::renderer::SetTemporalAAEnabled(false);
    wi::RenderPath3D render;wi::scene::CameraComponent camera;render.scene=&scene;render.camera=&camera;render.init(960,640,96);
    render.setDepthOfFieldEnabled(false);render.setMotionBlurEnabled(false);render.ResizeBuffers();
    renegade::runtime::RuntimeProjectileVisuals visuals;ProjectileSimulation simulation;
    const auto definitions=ListProjectileAssets(project.rootPath,project.projectId,error);
    auto arrowDefinition=std::find_if(definitions.begin(),definitions.end(),[](const auto& d) {
        return !d.meshAssetId.empty() && d.name=="Flaming Arrow - Stick";
    });
    if(arrowDefinition==definitions.end())return 162;
    auto arrow=*arrowDefinition;arrow.stuckLifetimeSeconds=60;arrow.stickOnImpact=true;
    arrow.flightEffects.clear();arrow.impactEffect=ProjectileEffectKind::None;
    if(!visuals.Prepare(project.rootPath,"",project.projectId,arrow)){std::cerr<<visuals.error;return 163;}
    const auto frame=[&](float dt) {
        wi::eventhandler::FireEvent(wi::eventhandler::EVENT_THREAD_SAFE_POINT,0);
        visuals.Sync(scene,simulation,dt);render.PreUpdate();render.Update(dt);
        visuals.RefreshImpactMarkPose(scene);
        render.PreRender();render.Render();wi::graphics::GetDevice()->SubmitCommandLists();
        wi::renderer::UpdateGPUSuballocator();Sleep(10);
    };
    for(size_t i=0;i<scene.animations.GetCount();++i)scene.animations[i].Stop();
    for(unsigned f=0;f<10;++f)frame(0);
    const auto receiver=receivers.front();const auto oi=scene.objects.GetIndex(receiver);
    const auto bounds=scene.aabb_objects[oi];const auto centre=bounds.getCenter();const auto extent=bounds.getHalfWidth();
    std::cout<<"Skinned receiver "<<receiver<<" bounds "<<centre.x<<","<<centre.y<<","<<centre.z
        <<" half "<<extent.x<<","<<extent.y<<","<<extent.z<<std::endl;
    camera.CreatePerspective(960,640,.01f,1000.f,DirectX::XMConvertToRadians(45.f));
    const float distance=std::max(1.f,extent.y*3.3f);
    const auto eye=XMVectorSet(centre.x,centre.y,centre.z-distance,1);
    camera.TransformCamera(XMMatrixInverse(nullptr,XMMatrixLookAtLH(eye,XMLoadFloat3(&centre),XMVectorSet(0,1,0,0))));camera.UpdateCamera();
    auto* mesh=scene.meshes.GetComponent(scene.objects.GetComponent(receiver)->meshID);
    std::cout<<"Mesh vertices "<<mesh->vertex_positions.size()<<" indices "<<mesh->indices.size()<<" armature "<<mesh->armatureID<<std::endl;
    for(size_t t=0;t<3 && t<mesh->indices.size();++t) {
        const auto q=scene.GetPositionOnSurface(receiver,mesh->indices[t],mesh->indices[t],mesh->indices[t],{0,0});
        std::cout<<"Vertex "<<mesh->indices[t]<<" surface "<<q.x<<","<<q.y<<","<<q.z<<std::endl;
    }
    wi::vector<uint8_t> diagnosticPng;wi::helper::saveTextureToMemoryFile(render.GetRenderResult3D(),"PNG",diagnosticPng);
    Put("BUILD/p3-animated-wounds-initial.png",diagnosticPng);
    unsigned created=0;
    for(const XMFLOAT2 target: {XMFLOAT2{0,.35f},XMFLOAT2{-.2f,.1f},XMFLOAT2{.15f,-.3f}}) {
        bool found=false;XMFLOAT3 contact{},normal{};
        for(float ox: {0.f,-.08f,.08f,-.15f,.15f,-.3f,.3f}) {
            XMFLOAT3 origin{centre.x+target.x*extent.x*.5f+ox,centre.y+target.y*extent.y*.5f,centre.z-distance};
            wi::vector<wi::scene::Scene::RayIntersectionResult> hits;
            scene.IntersectsAll(hits,wi::primitive::Ray(origin,{0,0,1},0,distance*2),
                wi::enums::FILTER_OPAQUE|wi::enums::FILTER_TRANSPARENT);
            for(const auto& hit:hits)if(hit.entity==receiver && hit.vertexID0>=0) {
                contact=hit.position;normal=hit.normal;found=true;break;
            }
            if(found)break;
        }
        if(!found){std::cerr<<"No ray contact for target "<<target.x<<","<<target.y<<std::endl;return 135;}
        ProjectileImpact impact;impact.projectileId=100+created;impact.contact.position={contact.x,contact.y,contact.z};
        impact.contact.normal={normal.x,normal.y,normal.z};impact.contact.surfaceType=ImpactSurfaceType::Character;
        const ProjectileVector angles[]={{0,0,10},{4,1,9},{-5,-1,8}};
        impact.incomingVelocity=angles[created];
        if(!visuals.Spawn(scene,arrow.assetId,impact.projectileId,true))return 164;
        const size_t before=visuals.impactDecals.size();visuals.PresentSurfaceImpact(scene,impact,receiver);
        if(visuals.impactDecals.size()!=before+2)return 136;
        visuals.Impact(scene,impact,receiver);
        if(visuals.retained.size()!=created+1 || visuals.retained.back().skinAnchor.receiver!=receiver)return 165;
        ++created;
    }
    const auto capture=[&](const std::string& name) {
        wi::vector<uint8_t> png;if(!wi::helper::saveTextureToMemoryFile(render.GetRenderResult3D(),"PNG",png))return false;
        std::ofstream out("BUILD/p3-animated-wounds-"+name+".png",std::ios::binary);
        out.write(reinterpret_cast<const char*>(png.data()),png.size());return bool(out);
    };
    for(unsigned f=0;f<280;++f)frame(.016f);
    if(!capture("rest"))return 137;
    unsigned clips=0;float maxLag=0,maxTravel=0,maxSamePoseError=0;
    for(const std::string wanted: {"walking","swiping","run"}) {
        wi::ecs::Entity selected=wi::ecs::INVALID_ENTITY;
        for(size_t i=0;i<scene.animations.GetCount();++i) {
            scene.animations[i].Stop();const auto e=scene.animations.GetEntity(i);
            const auto* n=scene.names.GetComponent(e);
            if(selected==wi::ecs::INVALID_ENTITY && n && n->name.find(wanted)!=std::string::npos &&
                std::find(rigClips.begin(),rigClips.end(),e)!=rigClips.end())selected=e;
        }
        if(selected==wi::ecs::INVALID_ENTITY)return 138;
        auto* animation=scene.animations.GetComponent(selected);animation->amount=1;animation->SetLooped();animation->RootMotionOff();animation->Play();
        const auto first=scene.transforms.GetComponent(visuals.impactDecals.front().entity)->GetPosition();
        float lag=0,steadyLag=0,travel=0;
        for(unsigned f=0;f<180;++f) {
            visuals.Sync(scene,simulation,0);
            for(const auto& mark:visuals.impactDecals)if(mark.bleedAge<0) {
                const auto& a=mark.anchor;const auto expected=scene.GetPositionOnSurface(a.receiver,a.a,a.b,a.c,a.bary);
                const auto actual=scene.transforms.GetComponent(mark.entity)->GetPosition();
                maxSamePoseError=std::max(maxSamePoseError,XMVectorGetX(XMVector3Length(XMLoadFloat3(&expected)-XMLoadFloat3(&actual))));
            }
            frame(.016f);if(visuals.impactDecals.size()!=6)return 139;
            if(visuals.retained.size()!=3)return 166;
            for(const auto& embedded:visuals.retained) {
                XMFLOAT4X4 surface;
                if(!renegade::runtime::RuntimeImpactMarks::SurfacePose(scene,embedded.skinAnchor,surface))return 167;
                XMFLOAT4X4 expected;XMStoreFloat4x4(&expected,
                    XMLoadFloat4x4(&embedded.surfaceRelativePose)*XMLoadFloat4x4(&surface));
                const auto* rootTransform=scene.transforms.GetComponent(embedded.root);
                for(unsigned r=0;r<4;++r)for(unsigned col=0;col<4;++col)
                    if(std::abs(expected.m[r][col]-rootTransform->world.m[r][col])>.00003f)return 168;
                for(size_t o=0;o<scene.objects.GetCount();++o) {
                    const auto entity=scene.objects.GetEntity(o);
                    if(entity!=embedded.root && !scene.Entity_IsDescendant(entity,embedded.root))continue;
                    const auto* tr=scene.transforms.GetComponent(entity);
                    for(unsigned r=0;r<4;++r)for(unsigned col=0;col<4;++col)
                        if(std::abs(scene.matrix_objects[o].m[r][col]-tr->world.m[r][col])>.00003f)return 169;
                    ShaderMeshInstance gpu;std::memcpy(&gpu,scene.instanceArrayMapped+o,sizeof(gpu));
                    ShaderTransform expectedRaw;expectedRaw.Create(tr->world);
                    if(std::memcmp(&gpu.transformRaw,&expectedRaw,sizeof(expectedRaw))!=0)return 170;
                    const auto centre=scene.aabb_objects[o].getCenter();
                    if(XMVectorGetX(XMVector3Length(XMLoadFloat3(&centre)-XMLoadFloat3(&gpu.center)))>.00003f)return 171;
                }
            }
            for(const auto& mark:visuals.impactDecals)if(mark.bleedAge>=0) {
                const auto& a=mark.anchor;const auto* tr=scene.transforms.GetComponent(mark.entity);
                const auto point=scene.GetPositionOnSurface(a.receiver,a.a,a.b,a.c,a.bary);
                const auto world=XMLoadFloat4x4(&tr->world);
                const auto top=world.r[3]+world.r[1];
                if(XMVectorGetX(XMVector3Length(top-XMLoadFloat3(&point)))>.003f)return 145;
                const auto* decal=scene.decals.GetComponent(mark.entity);
                if(!decal || std::abs(decal->world._41-tr->world._41)>.00001f ||
                    std::abs(decal->world._42-tr->world._42)>.00001f ||
                    std::abs(decal->world._43-tr->world._43)>.00001f)return 146;
            }

            for(const auto& mark:visuals.impactDecals)if(mark.bleedAge<0) {
                const auto& a=mark.anchor;const auto expected=scene.GetPositionOnSurface(a.receiver,a.a,a.b,a.c,a.bary);
                const auto actual=scene.transforms.GetComponent(mark.entity)->GetPosition();
                const float error=XMVectorGetX(XMVector3Length(XMLoadFloat3(&expected)-XMLoadFloat3(&actual)));
                lag=std::max(lag,error);if(f>2)steadyLag=std::max(steadyLag,error);
                const auto* decal=scene.decals.GetComponent(mark.entity);
                const auto index=scene.decals.GetIndex(mark.entity);
                const auto centre=scene.aabb_decals[index].getCenter();
                if(!decal || std::abs(decal->world._41-actual.x)>.00001f ||
                    std::abs(decal->world._42-actual.y)>.00001f || std::abs(decal->world._43-actual.z)>.00001f ||
                    XMVectorGetX(XMVector3Length(XMLoadFloat3(&centre)-XMLoadFloat3(&actual)))>.0001f)return 144;
                if(f==70)std::cout<<"Contact sample expected "<<expected.x<<","<<expected.y<<","<<expected.z
                    <<" actual "<<actual.x<<","<<actual.y<<","<<actual.z<<" error "<<error<<std::endl;
            }
            const auto current=scene.GetPositionOnSurface(receiver,visuals.impactDecals.front().anchor.a,
                visuals.impactDecals.front().anchor.b,visuals.impactDecals.front().anchor.c,visuals.impactDecals.front().anchor.bary);
            travel=std::max(travel,XMVectorGetX(XMVector3Length(XMLoadFloat3(&current)-XMLoadFloat3(&first))));
            if((f==30 || f==70 || f==110) && !capture(wanted+"-"+std::to_string(f)))return 140;
        }
        maxLag=std::max(maxLag,lag);maxTravel=std::max(maxTravel,travel);++clips;
        std::cout<<"Native animated "<<wanted<<" wound pose lag metres "<<lag<<"; steady pose lag "<<steadyLag<<"; contact travel metres "<<travel<<std::endl;
    }
    if(clips!=3 || maxTravel<.01f || maxSamePoseError>.003f || maxLag>.003f)return 141;
    for(size_t i=0;i<scene.animations.GetCount();++i)scene.animations[i].Pause();
    frame(0);frame(0);
    for(auto& mark:visuals.impactDecals)if(!renegade::runtime::RuntimeImpactMarks::Follow(scene,mark.entity,mark.anchor))return 142;
    std::cout<<"Same completed pose anchor accuracy PASS "<<maxSamePoseError<<" metres"<<std::endl;
    std::cout<<"Three live skeletal clips; six wound/trail anchors retained; pause refresh PASS; max pose lag "<<maxLag<<std::endl;
    std::cout<<"Current rendered pose attachment and clip-loop transitions PASS"<<std::endl;
    scene.Entity_Remove(receiver);visuals.Sync(scene,simulation,0);if(!visuals.impactDecals.empty() || !visuals.retained.empty())return 143;
    std::cout<<"Three rigid arrows retain angled skin-relative pose and current GPU transforms through walking swiping running PASS"<<std::endl;
    std::cout<<"Removed skinned receiver retires wounds trails and arrows PASS"<<std::endl;
    return 0;
}

static int VerifyDebrisScene(const fs::path& descriptor,const fs::path& folder={},bool install=false)
{
    HWND window=CreateWindowExW(0,L"STATIC",L"Debris verification",WS_OVERLAPPEDWINDOW,
        0,0,960,640,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    wi::Application app;app.allow_hdr=false;app.SetWindow(window);
    wi::initializer::InitializeComponentsImmediate();
    struct Drain { ~Drain(){while(wi::renderer::IsPipelineCreationActive())Sleep(10);wi::graphics::GetDevice()->WaitForGPU();} } drain;
    ProjectService projects;ProjectMetadata project;std::string error;
    if(!projects.InspectProject(descriptor.generic_u8string(),project,error) ||
        !projects.OpenProject(descriptor.generic_u8string()))return 80;
    auto scenes=std::make_unique<SceneService>();SelectionService selection;CommandService commands;
    SceneDocumentService documents(*scenes,selection,commands,projects);
    const auto scenePath=(fs::u8path(project.rootPath)/project.startupScene).generic_u8string();
    if(!documents.Open(scenePath))return 81;
    auto& scene=scenes->GetScene();
    if(!RestoreMaterialTextureBindings(scene,project.rootPath,project.projectId).succeeded)return 82;
    using Geometry=renegade::runtime::RuntimeImpactGeometry;
    const Geometry::Kind kinds[]={Geometry::Kind::Glass,Geometry::Kind::Rock,Geometry::Kind::Wood};
    const char* names[]={"Glass","Rock","Wood"};
    StableId ids[3];
    if(install)for(unsigned i=0;i<3;++i) {
        const auto payload=Bytes(folder/(std::string(names[i])+".png"));
        const std::string base=std::string("Textures/Impacts/Debris/Knife")+names[i];
        ResourceAssetImportRequest request;request.projectRoot=project.rootPath;request.projectId=project.projectId;
        request.sourceProjectRelativePath="SourceAssets/"+base+".png";
        request.assetProjectRelativePath="Content/"+base+".rasset";request.expectedFormat=ResourceSourceFormat::Png;
        const auto product=fs::u8path(project.rootPath)/request.assetProjectRelativePath;
        if(fs::exists(product)) {
            ResourceAssetDocument document;
            if(!ReadResourceAssetDocument(product.generic_u8string(),document,error) || document.payload!=payload)return 83;
            ids[i]=document.manifest.assetId;
        } else {
            Put(fs::u8path(project.rootPath)/request.sourceProjectRelativePath,payload);
            fs::create_directories(product.parent_path());
            const auto imported=ResourceAssetService().ImportResourceAsset(request);
            if(!imported.succeeded){std::cerr<<imported.error;return 84;}ids[i]=imported.assetId;
        }
        auto donor=Geometry::FindDebrisMaterial(scene,kinds[i]);
        if(donor==wi::ecs::INVALID_ENTITY) {
            donor=scene.Entity_CreateTransform(std::string("Knife ")+names[i]+" impact resource");
            scene.materials.Create(donor);
        }
        PreparedMaterialTextureAsset prepared;
        if(!PrepareMaterialTextureAsset(project.rootPath,project.projectId,ids[i],prepared,error) ||
            !ApplyPreparedMaterialTextureAsset(scene,donor,MaterialTextureSlot::BaseColor,prepared,{},error))
            {std::cerr<<error;return 85;}
        auto& material=*scene.materials.GetComponent(donor);
        material.baseColor={1,1,1,i==0?.65f:1.f};material.userBlendMode=wi::enums::BLENDMODE_ALPHA;
        material.SetDoubleSided(true);material.SetCastShadow(false);material.emissiveColor={0,0,0,0};
        material.SetRoughness(i==0?.06f:.8f);material.SetReflectance(.04f);
        material.SetTransmissionAmount(i==0?.95f:0);material.SetRefractionAmount(i==0?.006f:0);
        material.SetDirty();
        auto* metadata=scene.metadatas.GetComponent(donor);
        if(!metadata)metadata=&scene.metadatas.Create(donor);
        metadata->bool_values.set(Geometry::DebrisKey(kinds[i]),true);
    }
    if(install) {
        if(!documents.Save(scenePath) || !documents.Reload())return 86;
        if(!RestoreMaterialTextureBindings(scene,project.rootPath,project.projectId).succeeded)return 87;
    }
    for(unsigned i=0;i<3;++i) {
        const auto donor=Geometry::FindDebrisMaterial(scene,kinds[i]);
        if(donor==wi::ecs::INVALID_ENTITY)return 88;
        if(install && scene.metadatas.GetComponent(donor)->string_values.get(
            MaterialBaseColorTextureAssetIdMetadataKey)!=ids[i])return 89;
        std::cout<<names[i]<<" governed texture valid after reload"<<std::endl;
    }
    for(size_t i=0;i<scene.characters.GetCount();++i)scene.characters[i].SetActive(false);
    wi::renderer::SetTemporalAAEnabled(false);
    wi::RenderPath3D render;wi::scene::CameraComponent camera;
    render.scene=&scene;render.camera=&camera;render.init(960,640,96);
    render.setDepthOfFieldEnabled(false);render.setMotionBlurEnabled(false);render.ResizeBuffers();
    camera.CreatePerspective(960,640,.01f,100.f,DirectX::XMConvertToRadians(55.f));
    camera.TransformCamera(DirectX::XMMatrixInverse(nullptr,DirectX::XMMatrixLookAtLH(
        DirectX::XMVectorSet(0,1.4f,2.1f,1),DirectX::XMVectorSet(0,1.2f,3.1f,1),DirectX::XMVectorSet(0,1,0,0))));
    camera.UpdateCamera();
    const auto frame=[&]() {
        wi::eventhandler::FireEvent(wi::eventhandler::EVENT_THREAD_SAFE_POINT,0);
        render.PreUpdate();render.Update(.016f);render.PreRender();render.Render();
        wi::graphics::GetDevice()->SubmitCommandLists();wi::renderer::UpdateGPUSuballocator();Sleep(10);
    };
    for(unsigned i=0;i<20;++i)frame();
    const ImpactSurfaceType surfaces[]={ImpactSurfaceType::Glass,ImpactSurfaceType::Stone,ImpactSurfaceType::Wood};
    for(unsigned i=0;i<3;++i) {
        renegade::runtime::RuntimeProjectileVisuals visuals;
        renegade::bridge::ProjectileImpact impact;impact.projectileId=17;
        impact.contact.position={0,1.2f,3.1f};impact.contact.normal={0,0,-1};
        impact.contact.surfaceType=surfaces[i];impact.incomingVelocity={0,0,10};
        visuals.PresentSurfaceImpact(scene,impact,wi::ecs::INVALID_ENTITY);
        const size_t expected=i==0?12:i==1?14:18;
        if(visuals.impactGeometry.pieces.size()!=expected)return 90;
        for(const auto& piece:visuals.impactGeometry.pieces) {
            const auto* mesh=scene.meshes.GetComponent(piece.entity);
            if(!mesh || mesh->vertex_uvset_0.size()!=4 || mesh->indices.size()!=6)return 91;
        }
        renegade::bridge::ProjectileSimulation simulation;
        for(unsigned n=0;n<90;++n) {
            visuals.Sync(scene,simulation,.016f);frame();
            if(n==5 || n==12 || n==20) {
                wi::vector<uint8_t> png;
                if(!wi::helper::saveTextureToMemoryFile(render.GetRenderResult3D(),"PNG",png))return 92;
                std::ofstream out("BUILD/p3-knife-"+std::string(names[i])+"-"+std::to_string(n)+".png",std::ios::binary);
                out.write(reinterpret_cast<const char*>(png.data()),png.size());
            }
        }
        if(!visuals.impactGeometry.pieces.empty())return 93;
        visuals.Reset(scene);
    }
    std::cout<<"Three native debris sequences, UV shapes and expiry PASS"<<std::endl;
    return 0;
}

static int VerifyWaterScene(const fs::path& descriptor,bool install)
{
    HWND window=CreateWindowExW(0,L"STATIC",L"Water impact verification",
        WS_OVERLAPPEDWINDOW,0,0,960,640,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    wi::Application app;app.allow_hdr=false;app.SetWindow(window);
    wi::initializer::InitializeComponentsImmediate();
    struct Drain { ~Drain(){while(wi::renderer::IsPipelineCreationActive())Sleep(10);
        wi::graphics::GetDevice()->WaitForGPU();} } drain;
    ProjectService projects;ProjectMetadata project;std::string error;
    if(!projects.InspectProject(descriptor.generic_u8string(),project,error) ||
       !projects.OpenProject(descriptor.generic_u8string()))return 60;
    auto scenes=std::make_unique<SceneService>();SelectionService selection;CommandService commands;
    SceneDocumentService documents(*scenes,selection,commands,projects);
    const auto scenePath=(fs::u8path(project.rootPath)/project.startupScene).generic_u8string();
    if(!documents.Open(scenePath))return 61;
    auto& scene=scenes->GetScene();
    auto pool=wi::ecs::INVALID_ENTITY;
    for(size_t i=0;i<scene.names.GetCount();++i)
        if(scene.names[i].name=="Water impact proof pool")pool=scene.names.GetEntity(i);
    if(pool==wi::ecs::INVALID_ENTITY) {
        pool=scene.Entity_CreatePlane("Water impact proof pool");
        auto* t=scene.transforms.GetComponent(pool);
        t->Scale(XMFLOAT3{1.15f,1,1.15f});t->Translate(XMFLOAT3{3.6f,.045f,3.8f});t->UpdateTransform();
        auto* m=scene.materials.GetComponent(pool);
        m->shaderType=wi::scene::MaterialComponent::SHADERTYPE_WATER;
        m->baseColor={.92f,.97f,1,1};m->SetRoughness(.06f);
        m->SetReflectance(.02f);m->SetRefractionAmount(.08f);m->SetNormalMapStrength(1);
        m->SetCastShadow(false);m->SetDirty();
        if(!ApplyObjectImpactSurface(scene,pool,ImpactSurfaceType::Water))return 62;
    }
    // A light and patterned basin make real reflection/refraction inspectable.
    bool hasLight=false,hasBasin=false;
    for(size_t i=0;i<scene.names.GetCount();++i) {
        hasLight|=scene.names[i].name=="Water proof light";
        hasBasin|=scene.names[i].name=="Water proof basin";
    }
    if(!hasLight) {
        const auto light=scene.Entity_CreateLight("Water proof light",{3.3f,2.5f,3.0f},{1,.97f,.9f},90,5);
        scene.lights.GetComponent(light)->SetType(wi::scene::LightComponent::POINT);
    }
    for(size_t i=0;i<scene.names.GetCount();++i)
        if(scene.names[i].name=="Water proof light")
            scene.lights.GetComponent(scene.names.GetEntity(i))->intensity=6;
    if(!hasBasin) {
        const auto basin=scene.Entity_CreateCube("Water proof basin");
        auto* t=scene.transforms.GetComponent(basin);
        t->Scale(XMFLOAT3{1.18f,.012f,1.18f});t->Translate(XMFLOAT3{3.6f,.014f,3.8f});t->UpdateTransform();
        scene.materials.GetComponent(basin)->baseColor={.045f,.16f,.23f,1};
        for(unsigned i=0;i<8;++i) {
            const auto tile=scene.Entity_CreateCube("Water proof basin stripe");
            t=scene.transforms.GetComponent(tile);
            t->Scale(XMFLOAT3{.055f,.003f,1.12f});
            t->Translate(XMFLOAT3{2.65f+float(i)*.27f,.029f,3.8f});t->UpdateTransform();
            scene.materials.GetComponent(tile)->baseColor={.35f,.48f,.55f,1};
        }
    }
    if(install) {
        if(!documents.Save(scenePath) || !documents.Reload())return 63;
        pool=wi::ecs::INVALID_ENTITY;
        for(size_t i=0;i<scene.names.GetCount();++i)
            if(scene.names[i].name=="Water impact proof pool")pool=scene.names.GetEntity(i);
        if(pool==wi::ecs::INVALID_ENTITY || CaptureObjectImpactSurface(scene,pool)!=ImpactSurfaceType::Water ||
            scene.materials.GetComponent(pool)->shaderType!=wi::scene::MaterialComponent::SHADERTYPE_WATER)return 64;
        std::cout<<"Water proof pool saved/reloaded with native shader and semantic classification"<<std::endl;
    }
    for(size_t i=0;i<scene.characters.GetCount();++i) {
        if(auto* t=scene.transforms.GetComponent(scene.characters.GetEntity(i))) {
            t->UpdateTransform();scene.characters[i].SetPosition(t->GetPosition());
        }
        scene.characters[i].gravity=0;
    }
    wi::renderer::SetTemporalAAEnabled(false);
    wi::RenderPath3D render;wi::scene::CameraComponent camera;
    render.scene=&scene;render.camera=&camera;render.init(960,640,96);
    render.setDepthOfFieldEnabled(false);render.setMotionBlurEnabled(false);render.ResizeBuffers();
    camera.CreatePerspective(960,640,.01f,100.f,XMConvertToRadians(55.f));
    camera.TransformCamera(XMMatrixInverse(nullptr,XMMatrixLookAtLH(
        XMVectorSet(3.6f,1.35f,1.7f,1),XMVectorSet(3.6f,.16f,3.8f,1),XMVectorSet(0,1,0,0))));
    camera.UpdateCamera();
    renegade::runtime::RuntimeProjectileVisuals visuals;
    renegade::bridge::ProjectileSimulation simulation;
    for(unsigned frame=0;frame<20;++frame) {
        render.PreUpdate();render.Update(.016f);render.PreRender();render.Render();
        wi::graphics::GetDevice()->SubmitCommandLists();wi::renderer::UpdateGPUSuballocator();
    }
    renegade::bridge::ProjectileImpact impact;
    impact.contact.position={3.6f,.045f,3.8f};impact.contact.normal={0,1,0};
    impact.contact.surfaceType=ImpactSurfaceType::Water;impact.incomingVelocity={0,-8,5};
    visuals.PresentSurfaceImpact(scene,impact,pool);
    if(scene.waterRipples.empty() || !scene.waterRipples.back().GetTexture()->IsValid())return 65;
    const float size=scene.waterRipples.back().params.siz.x;
    scene.waterRipples.back().Update(0);
    if(scene.waterRipples.back().params.siz.x!=size)return 66;
    for(unsigned frame=0;frame<110;++frame) {
        visuals.Sync(scene,simulation,.016f);
        wi::eventhandler::FireEvent(wi::eventhandler::EVENT_THREAD_SAFE_POINT,0);
        render.PreUpdate();render.Update(.016f);render.PreRender();render.Render();
        wi::graphics::GetDevice()->SubmitCommandLists();wi::renderer::UpdateGPUSuballocator();
        if(frame==5 || frame==12 || frame==20 || frame==35 || frame==60 || frame==90) {
            wi::vector<uint8_t> png;
            if(!wi::helper::saveTextureToMemoryFile(render.GetRenderResult3D(),"PNG",png))return 67;
            std::ofstream file("BUILD/p3-water-native-"+std::to_string(frame)+".png",std::ios::binary);
            file.write(reinterpret_cast<const char*>(png.data()),png.size());
        }
    }
    if(!visuals.impactGeometry.pieces.empty())return 68;
    scene.PutWaterRipple(XMFLOAT3{3.9f,.045f,3.8f}); // unrelated native ripple must survive reset.
    visuals.impactGeometry.Water(scene,{3.6f,.045f,3.8f},{0,1,0},1);
    visuals.Reset(scene);
    if(scene.waterRipples.size()!=1 || scene.waterRipples[0].textureName==
        renegade::runtime::RuntimeImpactGeometry::RippleTag)return 69;
    std::cout<<"Native transmissive crown/drops and water shader ripples rendered; pause/reset isolation PASS"<<std::endl;
    return 0;
}

static std::vector<std::uint8_t> Wav()
{
    std::vector<std::uint8_t> p(44+48000*2);
    auto put=[&](unsigned at,unsigned v,unsigned n){for(unsigned i=0;i<n;++i)p[at+i]=(v>>(i*8))&255;};
    std::copy_n((const std::uint8_t*)"RIFF",4,p.begin());put(4,unsigned(p.size()-8),4);
    std::copy_n((const std::uint8_t*)"WAVEfmt ",8,p.begin()+8);put(16,16,4);put(20,1,2);
    put(22,1,2);put(24,48000,4);put(28,96000,4);put(32,2,2);put(34,16,2);
    std::copy_n((const std::uint8_t*)"data",4,p.begin()+36);put(40,96000,4);
    for(unsigned i=0;i<48000;++i)put(44+i*2,static_cast<std::uint16_t>(int(std::sin(i*.0576)*200)),2);
    return p;
}
static int VerifyNative(const fs::path& descriptor, bool tagMetal, bool stagePackage=false, ImpactSurfaceType tagType=ImpactSurfaceType::Metal, bool surfaceRange=false)
{
    // Scene dependency extraction uses native mesh resources and needs a device.
    HWND window=CreateWindowExW(0,L"STATIC",L"Renegade impact audio verification",
        WS_OVERLAPPEDWINDOW,0,0,64,64,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    wi::Application application; application.allow_hdr=false; application.SetWindow(window);
    wi::initializer::InitializeComponentsImmediate();
    struct Drain { ~Drain(){ while(wi::renderer::IsPipelineCreationActive()) Sleep(10);
        wi::graphics::GetDevice()->WaitForGPU(); } } drain;
    ProjectService projects; ProjectMetadata project; std::string error;
    if(!projects.InspectProject(descriptor.generic_u8string(),project,error))
    {std::cerr<<error;return 1;}
    const auto& dust=renegade::runtime::GetConcreteDustTexture();
    if(!dust.IsValid() || dust.GetDesc().width!=1254 || dust.GetDesc().height!=1254)
    {std::cerr<<"Embedded concrete dust decode failed";return 20;}
    std::cout<<"Embedded concrete dust: 1254x1254, native GPU texture valid"<<std::endl;
    if(!renegade::runtime::GetBloodSprayTexture().IsValid() ||
        !renegade::runtime::GetBloodSplatTexture().IsValid() ||
        !renegade::runtime::GetBloodSprayAtlasTexture().IsValid())
    {std::cerr<<"Embedded blood art decode failed";return 30;}
    std::cout<<"Embedded blood spray and splat: GPU decode valid"<<std::endl;
    // Verify every distributable core impact image is actually embedded and
    // decodable by Wicked in a native executable (not merely present on disk).
    {
        constexpr unsigned sides[]={4096,1254,2048,2048,2048,2048,2048,1254,2048,2048,1254,2048};
        for(unsigned i=0;i<static_cast<unsigned>(renegade::runtime::BuiltinImpactAtlas::Count);++i) {
            const auto& image=renegade::runtime::GetBuiltinImpactAtlas(
                static_cast<renegade::runtime::BuiltinImpactAtlas>(i));
            if(!image.IsValid() || image.GetDesc().width!=sides[i] ||
                image.GetDesc().height!=sides[i]) {
                std::cerr<<"Original impact atlas decode failed at index "<<i;
                return 36;
            }
        }
        std::cout<<"All 12 original impact atlases decoded as native GPU textures"<<std::endl;
    }
    {
        auto proof=std::make_unique<wi::scene::Scene>();
        const auto floor=proof->Entity_CreatePlane("Blood collision proof floor");
        renegade::runtime::RuntimeBloodEffects blood;
        blood.Spawn(*proof,{0,.15f,0},{0,-1,0},wi::ecs::INVALID_ENTITY,1.f,17);
        for(unsigned frame=0;frame<30;++frame)
        {proof->Update(.016f);blood.Update(*proof,.016f,{});}
        if(blood.stains.empty() || !blood.drops.empty())
        {std::cerr<<"Blood floor collision proof failed";return 31;}
        for(const auto& stain:blood.stains)
            if(proof->hierarchy.GetComponent(stain.entity)->parentID!=floor)return 32;
        std::cout<<"Blood native ray collision: "<<blood.stains.size()<<" floor stains; all droplets retired"<<std::endl;
        blood.Update(*proof,31,{});
        if(blood.stains.empty() || proof->materials.GetComponent(blood.stains.front().entity)->roughness<=.25f)
            return 35;
        std::cout<<"Blood native floor stains retained and drying after 31 seconds"<<std::endl;
        blood.Reset(*proof);
    }
    if(tagMetal || surfaceRange)
    {
        if(!projects.OpenProject(descriptor.generic_u8string()))return 2;
        auto scenes=std::make_unique<SceneService>();SelectionService selection;CommandService commands;
        SceneDocumentService documents(*scenes,selection,commands,projects);
        const auto path=(fs::u8path(project.rootPath)/project.startupScene).generic_u8string();
        if(!documents.Open(path))return 3;
        bool tagged=false;
        for(std::size_t i=0;i<scenes->GetScene().names.GetCount();++i)
            if(scenes->GetScene().names[i].name=="Projectile test wall")
                tagged=ApplyObjectImpactSurface(scenes->GetScene(),scenes->GetScene().names.GetEntity(i),tagType);
        if(surfaceRange)
        {
            auto& scene=scenes->GetScene();
            // Only the disposable playground is modified. Each factory cube owns its mesh/material.
            for(int target=0;target<5;++target)
            {
                const std::string name=target==0?"Impact comparison Metal":target==1?"Impact comparison Concrete":target==2?"Impact comparison Wood":
                    target==3?"Impact comparison Glass":"Impact comparison Water";
                wi::ecs::Entity entity=wi::ecs::INVALID_ENTITY;
                for(std::size_t i=0;i<scene.names.GetCount();++i)
                    if(scene.names[i].name==name)entity=scene.names.GetEntity(i);
                if(entity==wi::ecs::INVALID_ENTITY)entity=target==4?scene.Entity_CreatePlane(name):scene.Entity_CreateCube(name);
                auto* transform=scene.transforms.GetComponent(entity);
                if(!transform)return 21;
                transform->ClearTransform();
                transform->Scale(target==4?DirectX::XMFLOAT3{1.1f,1.f,1.1f}:DirectX::XMFLOAT3{.70f,1.0f,.04f});
                transform->Translate(target==4?DirectX::XMFLOAT3{4.8f,.25f,5.8f}:
                    DirectX::XMFLOAT3{target==3?-4.8f:float(target-1)*2.4f,1.2f,7.5f});
                transform->UpdateTransform();
                const auto type=target==0?ImpactSurfaceType::Metal:target==1?ImpactSurfaceType::Concrete:target==2?ImpactSurfaceType::Wood:
                    target==3?ImpactSurfaceType::Glass:ImpactSurfaceType::Water;
                if(!ApplyObjectImpactSurface(scene,entity,type))return 22;
                auto* object=scene.objects.GetComponent(entity);
                auto* mesh=object?scene.meshes.GetComponent(object->meshID):nullptr;
                if(!mesh || mesh->subsets.empty())return 23;
                auto* material=scene.materials.GetComponent(mesh->subsets[0].materialID);
                if(!material)return 24;
                material->baseColor=target==2?DirectX::XMFLOAT4{.40f,.22f,.09f,1}:DirectX::XMFLOAT4{.55f,.56f,.55f,1};
                material->SetRoughness(target==0?.3f:1.f);
                if(target==3 || target==4)
                {
                    material->baseColor=target==3?DirectX::XMFLOAT4{.80f,.91f,.96f,.24f}:
                        DirectX::XMFLOAT4{.10f,.24f,.30f,.85f};
                    material->userBlendMode=wi::enums::BLENDMODE_ALPHA;
                    material->SetRoughness(.08f);material->SetReflectance(1.f);
                    material->SetCastShadow(false);material->SetDoubleSided(true);mesh->SetDoubleSided(true);
                    if(target==3)material->SetRefractionAmount(.05f);
                    else material->shaderType=wi::scene::MaterialComponent::SHADERTYPE_WATER;
                }
                material->SetDirty();
            }
            wi::ecs::Entity dummy=wi::ecs::INVALID_ENTITY;
            for(size_t i=0;i<scene.names.GetCount();++i)
                if(scene.names[i].name=="Blood response Character dummy")dummy=scene.names.GetEntity(i);
            if(dummy==wi::ecs::INVALID_ENTITY)
            {
                dummy=scene.Entity_CreateTransform("Blood response Character dummy");
                scene.transforms.GetComponent(dummy)->Translate(DirectX::XMFLOAT3{0,0,5.5f});
                scene.transforms.GetComponent(dummy)->UpdateTransform();
                for(unsigned part=0;part<2;++part)
                {
                    const auto entity=scene.Entity_CreateSphere(part?"Blood dummy head":"Blood dummy torso",1,12,16);
                    auto* t=scene.transforms.GetComponent(entity);
                    t->Scale(part?DirectX::XMFLOAT3{.20f,.24f,.20f}:DirectX::XMFLOAT3{.38f,.70f,.25f});
                    t->Translate(DirectX::XMFLOAT3{0,part?1.85f:1.0f,0});t->UpdateTransform();
                    scene.Component_Attach(entity,dummy,true);
                    // Attach preserves world pose; these coordinates are authored relative to the dummy.
                    t=scene.transforms.GetComponent(entity);t->ClearTransform();
                    t->Scale(part?DirectX::XMFLOAT3{.20f,.24f,.20f}:DirectX::XMFLOAT3{.38f,.70f,.25f});
                    t->Translate(DirectX::XMFLOAT3{0,part?1.85f:1.0f,0});t->UpdateTransform();
                    auto* material=scene.materials.GetComponent(entity);
                    material->baseColor={.58f,.58f,.58f,1};material->SetRoughness(.85f);material->SetDirty();
                }
                CharacterAuthoringSettings settings;settings.role=CharacterRole::Passive;
                settings.autonomous=false;settings.canFlee=false;settings.canUseCover=false;
                MakeCharacterCommand promote(scene,dummy,settings);
                if(!promote.Execute())return 33;
            }
            if(!IsRenegadeCharacter(scene,dummy))return 34;
            std::cout<<"Governed passive Character blood dummy added"<<std::endl;
            tagged=true;
            std::cout<<"Comparison targets: Glass far left / Metal / Concrete / Wood / Water pool far right"<<std::endl;
        }
        if(!tagged || !documents.Save(path) || !documents.Open(path))return 4;
        if(surfaceRange)
        {
            auto& scene=scenes->GetScene();unsigned found=0;
            for(std::size_t i=0;i<scene.names.GetCount();++i)
                if(scene.names[i].name.rfind("Impact comparison ",0)==0)
                {
                    const auto entity=scene.names.GetEntity(i);
                    const auto& name=scene.names[i].name;
                    const auto expected=name=="Impact comparison Metal"?ImpactSurfaceType::Metal:
                        name=="Impact comparison Concrete"?ImpactSurfaceType::Concrete:
                        name=="Impact comparison Wood"?ImpactSurfaceType::Wood:
                        name=="Impact comparison Glass"?ImpactSurfaceType::Glass:ImpactSurfaceType::Water;
                    if(ResolveObjectImpactSurface(scene,entity)!=expected)return 26;
                    const auto* object=scene.objects.GetComponent(entity);
                    if(!object || object->meshID!=entity)return 27;
                    ++found;
                }
            if(found!=5)return 25;
            std::cout<<"Comparison scene save/reload: 5 targets"<<std::endl;
        }
        std::cout<<"Disposable test wall Surface Type = "<<ImpactSurfaceTypeToken(tagType)<<std::endl;
    }
    WindowsGameBuildProjectState state;
    if(!PrepareWindowsGameBuildProjectState(project,state,error)){std::cerr<<error;return 5;}

    if(stagePackage)
    {
        const auto root=fs::current_path();
        const auto runtime=root/"BUILD/renegade/Runtime/Release";
        const auto studio=root/"BUILD/renegade/Studio/Release";
        WindowsGameBuildStagingRequest staging;
        staging.projectRootPath=project.rootPath;
        staging.outputParentPath=(root/"BUILD/p3-impact-audio-export").generic_u8string();
        staging.stagingId="impact-audio-"+std::to_string(GetCurrentProcessId());
        staging.renegadeRevision="cf04bd38434dbc3e84c90ce22ee628e3c3953a80";
        staging.wickedRevision="3a800b7134aafe58461093c8abb2e274d4e64033";
        if(!SerializeAssetRegistry(state.assetRegistry,staging.assetRegistryJson,error))
        {std::cerr<<error;return 8;}
        std::vector<WindowsRuntimeSupportInput> support;
        for(const auto& name:{"RenegadeRuntime.exe","dxcompiler.dll"})
        {
            WindowsGamePackageFileDigest digest;
            const auto source=(runtime/name).generic_u8string();
            if(!DigestWindowsGamePackageFile(source,digest,error)){std::cerr<<error;return 9;}
            support.push_back({std::string(name)=="dxcompiler.dll"?"directx-shader-compiler":"renegade-runtime",
                name,digest.byteCount,digest.sha256,"p3:impact-audio-native-proof"});
            staging.runtimeSupportSources.push_back({name,source});
        }
        for(const auto& name:{"ReadMe.txt","Renegade-Licence-or-Notice.txt","WickedEngine-LICENSE.txt",
            "WickedEngine-third_party_software.txt","DirectXShaderCompiler-LICENSE.txt","DirectXShaderCompiler-ThirdPartyNotices.txt"})
            staging.packageDocuments.push_back({std::string(name)=="ReadMe.txt"?name:std::string("Licences/")+name,
                (studio/"BuildInputs"/name).generic_u8string(),
                std::string(name)=="ReadMe.txt"?"renegade-build-readme":
                std::string(name)=="Renegade-Licence-or-Notice.txt"?"renegade":
                std::string(name)=="WickedEngine-LICENSE.txt"?"wicked-engine":
                std::string(name)=="WickedEngine-third_party_software.txt"?"wicked-engine-third-party":
                std::string(name)=="DirectXShaderCompiler-LICENSE.txt"?"directx-shader-compiler":"directx-shader-compiler-third-party",
                std::string(name)=="ReadMe.txt" || std::string(name)=="Renegade-Licence-or-Notice.txt"?
                    "repo:"+staging.renegadeRevision:"pinned:"+staging.wickedRevision});
        WindowsGameBuildRequest request;
        request.gameName="Impact Audio Proof";request.executableBaseName="RenegadeRuntime";
        request.saveDataId=project.projectId;
        WindowsGameBuildPlan plan;WindowsGameBuildStageResult result;
        if(!CreateWindowsGameBuildPlan(project,state.dependencyGraph,state.assetRegistry,request,support,plan,error) ||
            !StageWindowsGameBuild(plan,staging,result,error) || !ValidateWindowsGameBuildStage(result,error))
        {std::cerr<<error;return 10;}
        // Self-contained one-pixel icon for the explicit manual package fixture.
        std::vector<std::uint8_t> icon;
        auto word=[&](unsigned n){icon.push_back(n&255);icon.push_back((n>>8)&255);};
        auto dword=[&](unsigned n){word(n&65535);word(n>>16);};
        word(0);word(1);word(1);icon.insert(icon.end(),{1,1,0,0});
        word(1);word(32);dword(48);dword(22);
        dword(40);dword(1);dword(2);word(1);word(32);
        dword(0);dword(4);dword(0);dword(0);dword(0);dword(0);
        icon.insert(icon.end(),{210,130,60,255,0,0,0,0});
        Put(root/"BUILD/p3-impact-audio-proof.ico",icon);
        WindowsGameExecutableIdentityRequest identity;
        identity.developerPublisher="Maverick Media Studio";
        identity.description="P3 impact audio package verification";
        identity.copyrightNotice="Copyright 2026 Maverick Media Studio";
        identity.internalBuildId="p3-impact-audio-local-proof";
        identity.buildTimestampUtc="2026-10-08T21:40:00Z";
        identity.iconSourcePath=(root/"BUILD/p3-impact-audio-proof.ico").generic_u8string();
        WindowsGameExecutableIdentityResult identified;
        if(!ApplyWindowsGameExecutableIdentity(plan,identity,result,identified,error) ||
            !ValidateWindowsGameBuildStage(result,error)){std::cerr<<error;return 11;}
        Text(root/"BUILD/p3-impact-audio-stage-path.txt",result.stagingPath);
        std::cout<<"STAGED_PACKAGE="<<result.stagingPath<<std::endl;
    }

    unsigned audio=0;bool hasBank=false;
    for(const auto& node:state.dependencyGraph.nodes)
    {if(node.dependencyClass==DependencyClass::Audio)++audio;if(node.projectRelativePath==ImpactAudioBankPath)hasBank=true;}
    std::cout<<"Native Build Game discovery audio="<<audio<<" bank="<<hasBank<<std::endl;
    wi::audio::Initialize();
    ImpactAudioPlayer player;
    if(!player.Prepare(project.rootPath,"",project.projectId,error)){std::cerr<<error;return 6;}
    const auto clips=player.ClipCount();player.Reset();
    std::cout<<"Native supplied WAV decode clips="<<clips<<std::endl;
    return audio>=14 && hasBank && clips==14?0:7;
}

// Private supplied art stays outside the source tree; export only governed impact resources.
static int ExportImpactLibrary(const fs::path& descriptor,const fs::path& destination)
{
    HWND window=CreateWindowExW(0,L"STATIC",L"Impact defaults export",WS_OVERLAPPEDWINDOW,
        0,0,960,640,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    wi::Application app;app.allow_hdr=false;app.SetWindow(window);
    wi::initializer::InitializeComponentsImmediate();
    struct Drain { ~Drain(){while(wi::renderer::IsPipelineCreationActive())Sleep(10);
        wi::graphics::GetDevice()->WaitForGPU();} } drain;
    ProjectService projects;ProjectMetadata project;std::string error;
    if(!projects.InspectProject(descriptor.generic_u8string(),project,error) ||
       !projects.OpenProject(descriptor.generic_u8string()))return 80;
    auto scenes=std::make_unique<SceneService>();SelectionService selection;CommandService commands;
    SceneDocumentService documents(*scenes,selection,commands,projects);
    if(!documents.Open((fs::u8path(project.rootPath)/project.startupScene).generic_u8string()))return 81;
    auto kit=std::make_unique<wi::scene::Scene>();std::set<StableId> needed;
    const auto& source=scenes->GetScene();
    for(size_t i=0;i<source.materials.GetCount();++i) {
        auto entity=source.materials.GetEntity(i);auto* metadata=source.metadatas.GetComponent(entity);
        if(!metadata)continue;
        bool impact=false;
        for(const auto& key:metadata->bool_values.names)
            if(IsImpactDefaultsMaterialKey(key) && metadata->bool_values.get(key))impact=true;
        if(!impact)continue;
        kit->materials.Create(entity)=source.materials[i];
        kit->metadatas.Create(entity)=*metadata;
        if(auto* name=source.names.GetComponent(entity))kit->names.Create(entity)=*name;
        for(const auto slot:{MaterialTextureSlot::BaseColor,MaterialTextureSlot::Normal,MaterialTextureSlot::Surface,
            MaterialTextureSlot::Emissive,MaterialTextureSlot::Occlusion}) {
            const auto key=MaterialTextureSlotMetadataKey(slot);
            if(metadata->string_values.has(key))needed.insert(metadata->string_values.get(key));
        }
    }
    ImpactAudioBank bank;AssetRegistry registry;
    if(!ReadImpactAudioBank(project.rootPath,project.projectId,bank,error) ||
       !ReadAssetRegistry(project.rootPath,project.projectId,registry,error)){std::cerr<<error;return 82;}
    for(const auto& surface:bank.surfaces)for(const auto& id:surface)needed.insert(id);
    fs::create_directories(destination);
    for(const auto& id:needed) {
        auto record=std::find_if(registry.records.begin(),registry.records.end(),
            [&](const AssetRecord& r){return r.assetId==id;});
        if(record==registry.records.end()){std::cerr<<"Missing library record "<<id;return 83;}
        Put(destination/fs::u8path(record->projectRelativePath),
            Bytes(fs::u8path(project.rootPath)/record->projectRelativePath));
    }
    if(!WriteAssetRegistry(destination.generic_u8string(),registry).success ||
       !WriteImpactAudioBank(destination.generic_u8string(),bank,error))return 84;
    wi::Archive archive;kit->Serialize(archive);
    if(!archive.SaveFile((destination/"Defaults.wiscene").generic_u8string()))return 85;
    Text(destination/"ImpactDefaults.renegade","format=renegade-project\nversion=1\n[project]\nproject_id="+
        project.projectId+"\nname=Impact Defaults\nstartup_scene=Defaults.wiscene\n");
    std::cout<<"Exported impact library donors="<<kit->materials.GetCount()<<" resources="<<needed.size()<<std::endl;
    return 0;
}
static int VerifyImpactDefaults(const fs::path& library,const fs::path& folder)
{
    HWND window=CreateWindowExW(0,L"STATIC",L"Ordinary project impact defaults",WS_OVERLAPPEDWINDOW,
        0,0,960,640,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    wi::Application app;app.allow_hdr=false;app.SetWindow(window);
    wi::initializer::InitializeComponentsImmediate();
    struct Drain { ~Drain(){while(wi::renderer::IsPipelineCreationActive())Sleep(10);
        wi::graphics::GetDevice()->WaitForGPU();} } drain;
    fs::create_directories(folder);
    auto empty=std::make_unique<wi::scene::Scene>();wi::Archive archive;empty->Serialize(archive);
    const auto starter=folder/"Starter.wiscene";
    if(!archive.SaveFile(starter.generic_u8string()))return 86;
    ProjectService projects;
    const auto name="Defaults Proof "+std::to_string(GetCurrentProcessId());
    if(!projects.CreateProject(folder.generic_u8string(),name,starter.generic_u8string()))return 87;
    const auto project=projects.CurrentProject();
    auto scenes=std::make_unique<SceneService>();SelectionService selection;CommandService commands;
    SceneDocumentService documents(*scenes,selection,commands,projects);
    auto& scene=scenes->GetScene();std::string error;
    WeatherState weather;weather.ambientIntensity=.4f;(void)CreateEnvironment(scene,weather);
    // Existing owner material is retained verbatim while missing categories are adopted.
    const auto custom=scene.Entity_CreateTransform("Owner custom default impact");
    scene.materials.Create(custom).baseColor={.12f,.34f,.56f,1};
    scene.metadatas.Create(custom).bool_values.set("renegade.impact.mark.default",true);
    auto command=PrepareImpactDefaults(scene,project,library.generic_u8string(),error);
    if(!command || !commands.Execute(std::move(command))){std::cerr<<error;return 88;}
    const auto count=scene.materials.GetCount();
    if(count!=12 || scene.objects.GetCount()!=0 || scene.meshes.GetCount()!=0 ||
       scene.materials.GetComponent(custom)->baseColor.x!=.12f)return 89;
    commands.Undo();
    if(scene.materials.GetCount()!=1)return 90;
    commands.Redo();
    if(scene.materials.GetCount()!=count)return 91;
    command=PrepareImpactDefaults(scene,project,library.generic_u8string(),error);
    if(!command || command->Execute() || scene.materials.GetCount()!=count)return 92;
    if(!documents.Save((fs::u8path(project.rootPath)/project.startupScene).generic_u8string()) ||
       !documents.Reload() || scene.materials.GetCount()!=count)return 93;
    const auto restore=RestoreMaterialTextureBindings(scene,project.rootPath,project.projectId);
    if(!restore.succeeded){std::cerr<<restore.error;return 94;}
    AssetRegistry registry;ImpactAudioBank bank;
    if(!ReadAssetRegistry(project.rootPath,project.projectId,registry,error) ||
       !ReadImpactAudioBank(project.rootPath,project.projectId,bank,error))return 95;
    size_t clips=0;for(const auto& surface:bank.surfaces)for(const auto& id:surface) {
        std::vector<uint8_t> payload;
        if(!PrepareImpactAudioAsset(project.rootPath,"",project.projectId,id,payload,error))return 96;
        ++clips;
    }
    if(clips!=14)return 97;
    for(const auto& record:registry.records) {
        if(fs::u8path(record.projectRelativePath).extension()!=".rasset")continue;
        ResourceAssetDocument resource;
        if(!ReadResourceAssetDocument((fs::u8path(project.rootPath)/record.projectRelativePath).generic_u8string(),
            resource,error) || resource.manifest.projectId!=project.projectId){std::cerr<<record.projectRelativePath<<": "<<error;return 98;}
    }

    const std::string flowHint="Content/StoryFlow/Main.renegade-flow";
    const auto flowPath=(fs::u8path(project.rootPath)/flowHint).generic_u8string();
    FlowDocument flow;flow.envelope=CreateDocumentEnvelope(project.projectId,StoryFlowDocumentType,
        flowHint,"impact-defaults-proof-v1");
    FlowNode start;start.id=GenerateStableId();start.kind=FlowNodeKind::GameStart;start.name="Game Start";
    flow.startNodeId=start.id;flow.nodes.push_back(start);
    if(!WriteFlowDocument(flowPath,flow,error))return 99;
    StoryFlowExistingLevelRequest request;request.projectRoot=project.rootPath;request.projectId=project.projectId;
    request.flowPath=flowPath;request.flow=flow;request.levelName="Impact defaults level";
    request.scenePath=(fs::u8path(project.rootPath)/project.startupScene).generic_u8string();
    const auto adopted=StoryFlowLevelReferenceService().AddExistingLevel(request);
    if(!adopted.succeeded){std::cerr<<adopted.message;return 100;}
    flow=adopted.committedFlow;FlowRoute route;route.id=GenerateStableId();route.sourceNodeId=start.id;
    route.outcome="renegade.flow.start";route.destinationNodeId=adopted.levelNodeId;route.destinationEntry="default";
    flow.routes.push_back(route);
    FlowNode complete;complete.id=GenerateStableId();complete.kind=FlowNodeKind::CompleteGame;
    complete.name="Complete Game";flow.nodes.push_back(complete);
    FlowRoute finish;finish.id=GenerateStableId();finish.sourceNodeId=adopted.levelNodeId;
    finish.outcome="level.complete";finish.destinationNodeId=complete.id;flow.routes.push_back(finish);
    if(!WriteFlowDocument(flowPath,flow,error))return 101;
    auto descriptorBytes=Bytes(project.descriptorPath);
    std::string descriptorText(descriptorBytes.begin(),descriptorBytes.end());
    for(const auto& pair:std::vector<std::pair<std::string,std::string>>{
        {"startup_flow_id = ","startup_flow_id = "+flow.envelope.documentId},
        {"startup_flow = ","startup_flow = "+flowHint}}) {
        auto pos=descriptorText.find(pair.first);
        if(pos==std::string::npos)return 102;
        descriptorText.replace(pos,pair.first.size(),pair.second);
    }
    Text(project.descriptorPath,descriptorText);
    Text(folder/"proof-project-path.txt",project.descriptorPath);
    std::cout<<"Ordinary project defaults PASS materials="<<count<<" resources="<<registry.records.size()
        <<" clips="<<clips<<" undo/redo/save/reload/custom preservation"<<std::endl;
    return 0;
}

int main(int argc,char** argv)
{
    _set_error_mode(_OUT_TO_STDERR);
    if(argc==4 && std::string(argv[1])=="--export-impact-library")return ExportImpactLibrary(argv[2],argv[3]);
    if(argc==4 && std::string(argv[1])=="--impact-defaults-proof")return VerifyImpactDefaults(argv[2],argv[3]);
    if(argc==4 && std::string(argv[1])=="--quiet-projectile")
    {
        ProjectMetadata project;std::string error;ProjectileAssetDocument projectile;
        if(!ProjectService().InspectProject(argv[2],project,error) ||
            !LoadProjectileAsset(project.rootPath,project.projectId,argv[3],projectile,error)){std::cerr<<error;return 1;}
        projectile.flightEffects.clear();projectile.impactEffect=ProjectileEffectKind::None;
        const auto saved=SaveProjectileAsset(project.rootPath,project.projectId,projectile);
        if(!saved.succeeded){std::cerr<<saved.error;return 2;}
        HWND window=CreateWindowExW(0,L"STATIC",L"Quiet arrow fixture",WS_OVERLAPPEDWINDOW,0,0,64,64,
            nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
        wi::Application application;application.allow_hdr=false;application.SetWindow(window);
        wi::initializer::InitializeComponentsImmediate();
        if(!ProjectService().InspectProject(argv[2],project,error)){std::cerr<<error;return 3;}
        ProjectService projects;if(!projects.OpenProject(argv[2]))return 4;
        auto scenes=std::make_unique<SceneService>();SelectionService selection;CommandService commands;
        SceneDocumentService documents(*scenes,selection,commands,projects);
        const auto path=(fs::u8path(project.rootPath)/project.startupScene).generic_u8string();
        if(!documents.Open(path))return 5;
        const auto start=ResolvePlayerStart(scenes->GetScene());
        if(start.resolution!=PlayerStartResolution::Success)return 6;
        EquipmentAssetDocument equipment;
        if(!LoadEquipmentAsset(project.rootPath,project.projectId,start.start.settings.primaryEquipmentAssetId,equipment,error))
        {std::cerr<<error;return 7;}
        for(auto& binding:equipment.equipment.projectiles)binding.projectileAssetId=saved.document.assetId;
        const auto weapon=SaveEquipmentAsset(project.rootPath,project.projectId,equipment.equipment);
        if(!weapon.succeeded){std::cerr<<weapon.error;return 8;}
        auto settings=start.start.settings;settings.primaryEquipmentAssetId=weapon.document.equipment.assetId;
        SetPlayerControllerSettingsCommand change(scenes->GetScene(),start.start.entity,settings);
        if(!change.Execute() || !documents.Save(path))return 9;
        while(wi::renderer::IsPipelineCreationActive())Sleep(10);
        wi::graphics::GetDevice()->WaitForGPU();
        std::cout<<"Quiet projectile assigned="<<saved.document.assetId<<std::endl;
        return 0;
    }

    if(argc==3 && std::string(argv[1])=="--surface-range")
        return VerifyNative(argv[2],false,false,ImpactSurfaceType::Concrete,true);

    if(argc==3 && (std::string(argv[1])=="--tag-wood" || std::string(argv[1])=="--tag-concrete"))
        return VerifyNative(argv[2],true,false,std::string(argv[1])=="--tag-wood"?ImpactSurfaceType::Wood:ImpactSurfaceType::Concrete);

    if(argc==4 && std::string(argv[1])=="--install-knife-marks")return VerifyMarksScene(argv[2],argv[3],true);
    if(argc==5 && std::string(argv[1])=="--install-mutant")return InstallPlaygroundMutant(argv[2],argv[3],argv[4]);
    if(argc==5 && std::string(argv[1])=="--animated-marks")return VerifyAnimatedMarks(argv[2],argv[3],argv[4]);
    if(argc==3 && std::string(argv[1])=="--marks-scene")return VerifyMarksScene(argv[2]);
    if(argc==4 && std::string(argv[1])=="--install-knife-debris")return VerifyDebrisScene(argv[2],argv[3],true);
    if(argc==3 && std::string(argv[1])=="--debris-scene")return VerifyDebrisScene(argv[2]);
    if(argc==4 && std::string(argv[1])=="--install-knife-blood")return VerifyBloodScene(argv[2],argv[3],true);
    if(argc==3 && std::string(argv[1])=="--blood-scene")return VerifyBloodScene(argv[2]);
    if(argc==3 && (std::string(argv[1])=="--water-scene" || std::string(argv[1])=="--install-water-proof"))
        return VerifyWaterScene(argv[2],std::string(argv[1])=="--install-water-proof");
    if(argc==3 && (std::string(argv[1])=="--verify" || std::string(argv[1])=="--tag-metal" || std::string(argv[1])=="--stage"))
        return VerifyNative(argv[2],std::string(argv[1])=="--tag-metal",std::string(argv[1])=="--stage");
    if(argc==5 && std::string(argv[1])=="--install") return Install(argv[2],argv[3],argv[4]);
    const auto fail=[](const std::string& e){std::cerr<<e<<"\n";return 1;};
    const auto root=fs::temp_directory_path()/("renegade-impact-audio-"+GenerateStableId());
    struct Cleanup { fs::path p; ~Cleanup(){std::error_code e;fs::remove_all(p,e);} } cleanup{root};
    fs::create_directories(root/"Content/Audio/Impacts");fs::create_directories(root/"Intermediate/Transactions");
    const auto projectId=GenerateStableId();std::string error;
    AssetRegistry registry; registry.projectId=projectId;
    if(!WriteAssetRegistry(root.generic_u8string(),registry).success)return fail("registry fixture");
    ImpactAudioBank bank;
    if(!ReadImpactAudioBank(root.generic_u8string(),projectId,bank,error))return fail("legacy missing bank");
    auto wav=Wav();
    if(!ValidateImpactAudioPayload(wav,error))return fail("valid PCM rejected");
    auto broken=wav;broken[16]=40;
    if(ValidateImpactAudioPayload(broken,error))return fail("oversized fmt accepted");
    broken=wav;broken.resize(40);
    if(ValidateImpactAudioPayload(broken,error))return fail("truncated WAV accepted");
    broken=wav;broken[40]=0xff;broken[43]=0x7f;
    if(ValidateImpactAudioPayload(broken,error))return fail("oversized data accepted");
    for(unsigned i=0;i<2;++i)
    {
        ResourceAssetImportRequest request;
        request.projectRoot=root.generic_u8string();request.projectId=projectId;
        request.sourceProjectRelativePath="SourceAssets/Audio/impact"+std::to_string(i)+".wav";
        request.assetProjectRelativePath="Content/Audio/Impacts/Metal/impact"+std::to_string(i)+".rasset";
        request.expectedFormat=ResourceSourceFormat::Wav;
        Put(root/request.sourceProjectRelativePath,wav);
        fs::create_directories(root/fs::u8path(request.assetProjectRelativePath).parent_path());
        auto imported=ResourceAssetService().ImportResourceAsset(request);
        if(!imported.succeeded)return fail(imported.error);
        bank.surfaces[1].push_back(imported.assetId);
    }
    if(!WriteImpactAudioBank(root.generic_u8string(),bank,error))return fail(error);
    ImpactAudioBank reopened;
    if(!ReadImpactAudioBank(root.generic_u8string(),projectId,reopened,error) ||
        reopened.surfaces!=bank.surfaces)return fail("bank roundtrip");
    auto wrong=bank;wrong.surfaces[1].push_back(wrong.surfaces[1][0]);
    if(WriteImpactAudioBank(root.generic_u8string(),wrong,error))return fail("duplicate accepted");
    if(ReadImpactAudioBank(root.generic_u8string(),GenerateStableId(),reopened,error))return fail("wrong project accepted");
    DependencyCollector collector(root.generic_u8string());
    if(!AddImpactAudioDependencies(root.generic_u8string(),projectId,collector,error) ||
        collector.Graph().nodes.size()!=3)return fail("missing bank/audio build closure: "+error);
    const auto snapshot=root/"snapshot";fs::create_directories(snapshot);
    if(!SnapshotImpactAudio(root.generic_u8string(),snapshot.generic_u8string(),projectId,error))return fail(error);
    std::vector<std::uint8_t> restored;
    if(!PrepareImpactAudioAsset(snapshot.generic_u8string(),"",projectId,bank.surfaces[1][0],restored,error) ||
        restored!=wav)return fail("snapshot depended on source WAV");
    // Package resolution consumes governed .rasset and manifest, no source files/registry.
    const auto package=root/"package";fs::create_directories(package/"GameData");
    if(!ReadAssetRegistry(root.generic_u8string(),projectId,registry,error))return fail(error);
    nlohmann::json manifest={{"format","renegade-content-manifest"},{"schema_version",1},
        {"project_id",projectId},{"files",nlohmann::json::array()}};
    for(const auto& id:bank.surfaces[1])
    {
        auto record=std::find_if(registry.records.begin(),registry.records.end(),[&](const AssetRecord& r){return r.assetId==id;});
        Put(package/"GameData"/record->projectRelativePath,Bytes(root/record->projectRelativePath));
        manifest["files"].push_back({{"asset_id",id},{"path","GameData/"+record->projectRelativePath},{"source_hash",record->contentHash}});
    }
    Text(package/"GameData/content-manifest.json",manifest.dump());
    if(!WriteImpactAudioBank((package/"GameData").generic_u8string(),bank,error))return fail(error);
    if(!PrepareImpactAudioAsset("",package.generic_u8string(),projectId,bank.surfaces[1][0],restored,error) ||
        restored!=wav)return fail("package lookup: "+error);
    if(PrepareImpactAudioAsset("",package.generic_u8string(),projectId,GenerateStableId(),restored,error))
        return fail("missing package product accepted");
    const bool nativeVoices=argc==2 && std::string(argv[1])=="--native-voices";
    if(!nativeVoices) {
        std::cout<<"ImpactAudioTests passed: governance, save/reload, snapshot, package"<<std::endl;
        return 0;
    }
    // CI workers can have XAudio2 but no render endpoint. Probe before Wicked's
    // asserting native initializer; only absent hardware is a CTest skip.
    const HRESULT com=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
    if(FAILED(com))return fail("audio preflight COM initialization failed");
    IMMDeviceEnumerator* enumerator=nullptr;IMMDeviceCollection* endpoints=nullptr;UINT endpointCount=0;
    HRESULT hr=CoCreateInstance(__uuidof(MMDeviceEnumerator),nullptr,CLSCTX_ALL,
        __uuidof(IMMDeviceEnumerator),reinterpret_cast<void**>(&enumerator));
    if(SUCCEEDED(hr))hr=enumerator->EnumAudioEndpoints(eRender,DEVICE_STATE_ACTIVE,&endpoints);
    if(SUCCEEDED(hr))hr=endpoints->GetCount(&endpointCount);
    if(endpoints)endpoints->Release();
    if(enumerator)enumerator->Release();
    CoUninitialize();
    if(FAILED(hr))return fail("audio render endpoint enumeration failed");
    // Explicit test-only seam to reproduce absent hardware on a developer PC.
    if(std::getenv("RENEGADE_TEST_NO_AUDIO_DEVICE"))endpointCount=0;
    if(endpointCount==0) {
        std::cout<<"SKIP native impact voices: no active audio render endpoint"<<std::endl;
        return 77;
    }
    wi::audio::Initialize();
    ImpactAudioPlayer player;
    if(!player.Prepare((package/"GameData").generic_u8string(),package.generic_u8string(),projectId,error) ||
        player.ClipCount()!=2)return fail("native prepare: "+error);
    wi::audio::SoundInstance3D listener;StableId played,previous;
    if(player.Play(ImpactSurfaceType::Default,{0,0,2},listener,played))return fail("invented default fallback");
    for(unsigned i=0;i<40;++i)
    {
        if(!player.Play(ImpactSurfaceType::Metal,{0,0,2},listener,played) || played==previous)return fail("native voice/variant");
        previous=played;
    }
    if(player.VoiceCount()>ImpactAudioPlayer::MaxVoices)return fail("unbounded voices");
    player.SetPaused(true);const auto count=player.VoiceCount();
    if(player.Play(ImpactSurfaceType::Metal,{0,0,2},listener,played))return fail("paused play");
    player.Update(20,listener);
    if(player.VoiceCount()!=count)return fail("pause consumed lifetime");
    player.SetPaused(false);player.Update(11,listener);
    if(player.VoiceCount()!=0)return fail("voices did not expire");
    player.StopVoices();
    if(player.ClipCount()!=2)return fail("screen discarded prepared bank");
    player.Reset();
    if(player.VoiceCount()!=0 || player.ClipCount()!=0)return fail("reset retained audio");
    std::cout<<"ImpactAudioTests passed: governance, save/reload, snapshot, package, native voices, variants, cap, pause/reset\n";
    return 0;
}
