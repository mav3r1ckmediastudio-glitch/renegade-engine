#include "renegade/bridge/RiverSplineService.h"
#include <filesystem>
#include <iostream>
#include <string>
#include <Windows.h>
#include <ModelImporter.h>
#include "renegade/bridge/RiverTerrainLayerService.h"
#include "renegade/bridge/IdentityService.h"
#include <cfloat>
#include <algorithm>
namespace fs=std::filesystem;
int main(int argc,char** argv)
{
    std::cout.setf(std::ios::unitbuf);
    std::cout<<"PROBE_ENTRY argc="<<argc<<"\n";
    if(argc!=2 && argc!=4)return 2;
    try {
        HWND window=CreateWindowExW(0,L"STATIC",L"Native River Scene Probe",WS_OVERLAPPEDWINDOW,
            0,0,800,600,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
        struct ProbeApp : wi::Application { void Initialize() override {
            wi::Application::Initialize();alwaysactive=true;} } app;
        app.allow_hdr=false;app.SetWindow(window);
        std::cout<<"PROBE_BEFORE_INIT\n";
        wi::initializer::InitializeComponentsImmediate();
        std::cout<<"PROBE_INIT_PASS\n";
        struct Drain { ~Drain(){while(wi::renderer::IsPipelineCreationActive())Sleep(10);
            wi::graphics::GetDevice()->WaitForGPU();} } drain;
        wi::resourcemanager::SetMode(wi::resourcemanager::Mode::EMBED_FILE_DATA);
        wi::scene::Scene scene;
        wi::Archive ar(fs::path(argv[1]).generic_u8string(),true);
        std::cout<<"PROBE_ARCHIVE_OPEN="<<ar.IsOpen()<<"\n";
        if(!ar.IsOpen()){std::cerr<<"ARCHIVE_OPEN_FAILED\n";return 3;}
        scene.Serialize(ar);
        std::cout<<"OBJECTS="<<scene.objects.GetCount()
                 <<" MATERIALS="<<scene.materials.GetCount()
                 <<" TRANSFORMS="<<scene.transforms.GetCount()<<"\n";
        if(auto* t=scene.terrains.GetComponent(scene.Entity_FindByName("Disposable Real Terrain"))) {
            float minimum=999999.f,maximum=-999999.f,closest=999999.f,closeHeight=999999.f;
            unsigned count=0;
            for(const auto& chunk:t->chunks) {
                const auto* mesh=scene.meshes.GetComponent(chunk.second.entity);
                if(!mesh)continue;
                const auto world=scene.ComputeEntityMatrixRecursive(chunk.second.entity);
                for(const auto& v:mesh->vertex_positions) {
                    DirectX::XMFLOAT3 pos;
                    DirectX::XMStoreFloat3(&pos,DirectX::XMVector3TransformCoord(
                        DirectX::XMLoadFloat3(&v),world));
                    const float dist=std::hypot(pos.x,pos.z-68.f);
                    if(dist<closest){closest=dist;closeHeight=pos.y;}
                    if(dist<3.f) {minimum=std::min(minimum,pos.y);
                        maximum=std::max(maximum,pos.y);++count;}
                }
            }
            std::cout<<"TERRAIN_AT_FOAM count="<<count<<" minY="<<minimum<<" maxY="
               <<maximum<<" nearestDist="<<closest<<" nearestY="<<closeHeight<<"\n";
        }
        for(size_t i=0;i<scene.names.GetCount();++i) {
            auto ent=scene.names.GetEntity(i);
            const auto& n=scene.names[i];
            auto* tr=scene.transforms.GetComponent(ent);
            std::cout<<"SCENE_ENTITY="<<ent<<" NAME="<<n.name
                <<" XYZ="<<(tr?tr->translation_local.x:-999)<<","<<(tr?tr->translation_local.y:-999)
                <<","<<(tr?tr->translation_local.z:-999)<<"\n";
        }
        for(size_t i=0;i<scene.materials.GetCount();++i) {
            auto e=scene.materials.GetEntity(i);
            const auto& m=scene.materials[i];
            if(m.shaderType != wi::scene::MaterialComponent::SHADERTYPE_WATER)continue;
            auto* n=scene.names.GetComponent(e);
            auto* t=scene.transforms.GetComponent(e);
            std::cout<<"WATER_ENTITY="<<e<<" NAME="<<(n?n->name:"(unnamed)")
              <<" Y="<<(t?t->translation_local.y:-999)
              <<" XYZ="<<(t?t->translation_local.x:-999)<<","<<(t?t->translation_local.z:-999)
              <<" WATER_COLOR="<<m.baseColor.x<<","<<m.baseColor.y<<","<<m.baseColor.z<<"\n";
            if(n && n->name=="River Water 5") {
                for(const auto node:scene.splines.GetComponent(e)->spline_node_entities){
                    auto* local=scene.transforms.GetComponent(node);
                    const auto matrix=scene.ComputeEntityMatrixRecursive(node);
                    DirectX::XMFLOAT3 wp;
                    DirectX::XMStoreFloat3(&wp, DirectX::XMVector3TransformCoord(
                        DirectX::XMVectorZero(),matrix));
                    std::cout<<"WATER5_NODE_WORLD="<<wp.x<<","<<wp.y<<","<<wp.z
                        <<" LOCAL="<<local->translation_local.x<<","
                        <<local->translation_local.y<<","<<local->translation_local.z<<"\n";
                }
            }
            auto* obj=scene.objects.GetComponent(e);
            const auto* mesh=obj?scene.meshes.GetComponent(obj->meshID):nullptr;
            if(mesh){
                DirectX::XMFLOAT3 lo{999999,999999,999999},hi{-999999,-999999,-999999};
                for(const auto& v:mesh->vertex_positions){
                    lo.x=std::min(lo.x,v.x);lo.y=std::min(lo.y,v.y);lo.z=std::min(lo.z,v.z);
                    hi.x=std::max(hi.x,v.x);hi.y=std::max(hi.y,v.y);hi.z=std::max(hi.z,v.z);
                }
                std::cout<<"WATER_MESH_VERTICES="<<mesh->vertex_positions.size()<<" BOUNDS="
                    <<lo.x<<","<<lo.y<<","<<lo.z<<" to "<<hi.x<<","<<hi.y<<","<<hi.z<<"\n";
            }
        }
        if(argc==4) {
            const auto terrainID=scene.Entity_FindByName("Disposable Real Terrain");
            auto* terr=scene.terrains.GetComponent(terrainID);
            std::cout<<"LEVEL_TERRAIN_STATE="<<(terr!=nullptr)<<" bound="
                <<(terr && terr->scene==&scene)<<" chunks="<<(terr?terr->chunks.size():0)
                <<" generation="<<(terr?terr->generation:0)
                <<" bottom="<<(terr?terr->bottomLevel:0)<<"\n";
            const fs::path target=fs::absolute(fs::u8path(argv[2]));
            const fs::path ownerRock=fs::absolute(fs::u8path(argv[3]));
            auto* water=scene.materials.GetComponent(scene.Entity_FindByName("River Water 5"));
            if(!water || water->shaderType!=wi::scene::MaterialComponent::SHADERTYPE_WATER) {
                std::cerr<<"EXPECTED_FLAT_WATER_MISSING\n";return 5;
            }
            // The old disposable scene predates the normal-map resource repair.
            // Use the current flat-river factory material in this test copy only.
            wi::scene::Scene stockScene;
            renegade::bridge::RiverSplineOptions stockOptions;
            stockOptions.widthMeters=4.f;stockOptions.surfaceOffsetMeters=0.f;
            renegade::bridge::CreateRiverSplineCommand stockCommand(stockScene,
                {{-2,0,0},{2,0,0}},stockOptions);
            if(!stockCommand.Execute()){std::cerr<<"NATIVE_WATER_FACTORY_FAILED\n";return 10;}
            *water=*stockScene.materials.GetComponent(stockCommand.CreatedEntity());
            water->SetDirty();
            // Other, sloped diagnostic rivers must not choose the reflection
            // plane for this flat-water-only visual fixture.
            const auto testedWater=scene.Entity_FindByName("River Water 5");
            for(size_t i=0;i<scene.materials.GetCount();++i)
                if(scene.materials[i].shaderType==wi::scene::MaterialComponent::SHADERTYPE_WATER
                    && scene.materials.GetEntity(i)!=testedWater)
                    if(auto* other=scene.objects.GetComponent(scene.materials.GetEntity(i)))
                        other->SetRenderable(false);
            // Only this disposable in-memory copy is altered. Mirror native
            // SceneService adoption before asking the governed river layer API
            // to sculpt the previously submerged flat riverbed.
            if(!terr || terr->chunks.size()!=9) {
                std::cerr<<"FOAM_TERRAIN_NOT_READY\n";return 8;
            }
            terr->terrainEntity=terrainID;
            terr->scene=&scene;
            std::string layerError;
            if(!renegade::bridge::EnsurePersistentEntityIdentities(scene,layerError)) {
                std::cerr<<"FOAM_STABLE_IDS_FAIL="<<layerError<<"\n";return 9;
            }
            // The saved baseline is already below this flat water's height.
            // Do NOT link/recompose terrain for the foam visual proof.
            std::cout<<"FOAM_TEST_TERRAIN_UNCHANGED=PASS\n";
            // Import owner-supplied rock without flattening/replacing any landscape.
            wi::scene::Scene rocks;
            ImportModel_GLTF(ownerRock.generic_u8string(),rocks);
            rocks.Update(0);
            if(rocks.objects.GetCount()==0){std::cerr<<"OWNER_ROCK_MISSING\n";return 6;}
            DirectX::XMFLOAT3 lo{FLT_MAX,FLT_MAX,FLT_MAX},hi{-FLT_MAX,-FLT_MAX,-FLT_MAX};
            for(const auto& box:rocks.aabb_objects){
                lo.x=std::min(lo.x,box._min.x);lo.y=std::min(lo.y,box._min.y);lo.z=std::min(lo.z,box._min.z);
                hi.x=std::max(hi.x,box._max.x);hi.y=std::max(hi.y,box._max.y);hi.z=std::max(hi.z,box._max.z);
            }
            const float scale=1.75f/std::max({hi.x-lo.x,hi.y-lo.y,hi.z-lo.z});
            const auto normal=DirectX::XMMatrixTranslation(-(lo.x+hi.x)*.5f,-lo.y,-(lo.z+hi.z)*.5f)
                *DirectX::XMMatrixScaling(scale,scale,scale)
                *DirectX::XMMatrixTranslation(0,-.35f,68);
            wi::vector<wi::ecs::Entity> roots;
            for(size_t i=0;i<rocks.transforms.GetCount();++i)
                if(!rocks.hierarchy.Contains(rocks.transforms.GetEntity(i))) roots.push_back(rocks.transforms.GetEntity(i));
            for(auto e:roots){auto* t=rocks.transforms.GetComponent(e);t->MatrixTransform(normal);t->UpdateTransform();}
            const size_t importedRockObjects=rocks.objects.GetCount();
            scene.Merge(rocks);
            // Disposable level only: put the existing Player Start beside the
            // flat-water proof so Runtime opens within sight of the test rock.
            const auto playerStart=scene.Entity_FindByName("Player Start");
            if(auto* spawn=scene.transforms.GetComponent(playerStart)) {
                spawn->translation_local={0,3,64};
                DirectX::XMStoreFloat4(&spawn->rotation_local,
                    DirectX::XMQuaternionRotationRollPitchYaw(0,0,0));
                spawn->SetDirty();
                std::cout<<"FOAM_TEST_PLAYER_START=0,3,64 yaw=0\n";
            }
            const fs::path textureRoot=target.parent_path().parent_path()/"river"/"foam";
            for(int i=0;i<2;++i){
                const auto e=scene.Entity_CreatePlane(i==0?"Rock Foam Contact - Test":"Rock Foam Wake - Test");
                auto* m=scene.materials.GetComponent(e);
                m->shaderType=wi::scene::MaterialComponent::SHADERTYPE_UNLIT;
                m->SetBaseColor({1,1,1,.93f});m->SetAlphaRef(1.f);
                m->SetCastShadow(false);m->SetDoubleSided(true);m->SetRoughness(1);
                m->userBlendMode=wi::enums::BLENDMODE_ALPHA;
                m->textures[wi::scene::MaterialComponent::BASECOLORMAP].name =
                    (textureRoot/(i==0?"river_foam_contact.png":"river_foam_wisps.png")).generic_u8string();
                m->textures[wi::scene::MaterialComponent::BASECOLORMAP].resource =
                    wi::resourcemanager::Load(m->textures[wi::scene::MaterialComponent::BASECOLORMAP].name);
                if(!m->textures[wi::scene::MaterialComponent::BASECOLORMAP].resource.IsValid()) {
                    std::cerr<<"FOAM_TEXTURE_MISSING\n";return 7;
                }
                m->texAnimFrameRate=60.f;
                m->texAnimDirection={0,i==0?-.00015f:-.0006f};m->SetDirty();
                auto* t=scene.transforms.GetComponent(e);
                t->translation_local={i==0?0.f:2.15f,.165f,68.f};
                t->RotateRollPitchYaw(DirectX::XMFLOAT3{0,DirectX::XM_PIDIV2,0});
                t->scale_local={i==0?1.2f:1.1f,1,i==0?1.15f:2.2f};
                t->SetDirty();
            }
            {wi::Archive output(target.generic_u8string(),false);scene.Serialize(output);}
            std::cout<<"ACTUAL_RIVER_FOAM_SCENE_WRITTEN="<<target.generic_u8string()
               <<" ROCKS="<<importedRockObjects<<" WATER_MATERIALS_PRESERVED=5\n";
        }
        return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 4;}
}
