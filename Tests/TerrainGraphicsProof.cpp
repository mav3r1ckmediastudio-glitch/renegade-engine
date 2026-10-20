#include "renegade/bridge/TerrainService.h"
#include "renegade/bridge/SceneDocumentService.h"
#include "renegade/bridge/VegetationService.h"
#include <Windows.h>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <map>
#include <cstdint>
using namespace renegade::bridge;
using namespace DirectX;
namespace fs = std::filesystem;
static void Require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
// Exact authored mask fingerprints, independent of chunk map iteration order.
static std::map<std::pair<uint64_t,int>,uint64_t> Masks(const wi::scene::Scene& scene,
    const wi::terrain::Terrain& terrain)
{
    std::map<std::pair<uint64_t,int>,uint64_t> result;
    auto hash=[](const auto& values){ uint64_t h=1469598103934665603ull;
        const auto* bytes=reinterpret_cast<const uint8_t*>(values.data());
        for(size_t i=0;i<values.size()*sizeof(values[0]);++i) h=(h^bytes[i])*1099511628211ull;
        return h;
    };
    for(const auto& entry:terrain.chunks){
        result[{entry.second.entity,0}]=hash(entry.second.grass.vertex_lengths);
        int layer=1;
        for(const auto& blend:entry.second.blendmap_layers)
            result[{entry.second.entity,layer++}]=hash(blend.pixels);
    }
    for(size_t i=0;i<scene.hairs.GetCount();++i)
        result[{scene.hairs.GetEntity(i),-1}]=hash(scene.hairs[i].vertex_lengths);
    return result;
}
int main(int argc, char** argv)
{
    if (argc != 3) { std::cerr << "Usage: TerrainGraphicsProof disposable-scene output-folder\n"; return 2; }
    if(fs::exists(fs::u8path(argv[2])) && !fs::is_empty(fs::u8path(argv[2]))) {
        std::cerr << "Output folder must be new or empty\n"; return 2;
    }
    fs::create_directories(fs::u8path(argv[2]));
    HWND window = CreateWindowExW(0,L"STATIC",L"Terrain isolated GPU proof",WS_OVERLAPPEDWINDOW,
        0,0,960,640,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    wi::Application app; app.allow_hdr=false; app.SetWindow(window);
    wi::initializer::InitializeComponentsImmediate();
    auto* device=wi::graphics::GetDevice();
    struct Drain { ~Drain(){ while(wi::renderer::IsPipelineCreationActive()) Sleep(10);
        wi::graphics::GetDevice()->WaitForGPU(); } } drain;
    try {
        auto prepared=PrepareWickedSceneOpen(argv[1]);
        Require(prepared.IsReady(),prepared.Error().c_str());
        auto& scene=*prepared.MutablePreparedScene();
        for(size_t i=0;i<scene.terrains.GetCount();++i){
            scene.terrains[i].terrainEntity=scene.terrains.GetEntity(i); scene.terrains[i].scene=&scene;
        }
        RebindDefaultTerrainMaterials(scene); RebindWickedVegetationResources(scene);
        Require(scene.terrains.GetCount()==1,"fixture requires one terrain");
        auto& terrain=scene.terrains[0]; terrain.Generation_Cancel();
        const size_t chunkCount=terrain.chunks.size(), hairCount=scene.hairs.GetCount();
        for(auto entity:terrain.materialEntities){ auto* m=scene.materials.GetComponent(entity);
            if(m) std::cout << "MATERIAL " << m->textures[0].name << " valid=" << m->textures[0].resource.IsValid() << " dirty=" << m->IsDirty() << std::endl;
        }
        const auto loadedHeights=CaptureTerrainSculpt(scene,terrain);
        const auto loadedMasks=Masks(scene,terrain);
        Require(chunkCount>0,"fixture has no terrain chunks");
        wi::scene::CameraComponent camera;
        camera.CreatePerspective(960,640,.1f,4000.f,XMConvertToRadians(60.f));
        camera.TransformCamera(XMMatrixInverse(nullptr,XMMatrixLookAtLH(
            XMVectorSet(50,55,-80,1),XMVectorSet(0,0,0,1),XMVectorSet(0,1,0,0)))); camera.UpdateCamera();
        wi::RenderPath3D render; render.scene=&scene; render.camera=&camera; render.init(960,640,96);
        render.setMotionBlurEnabled(false); render.setDepthOfFieldEnabled(false); render.ResizeBuffers();
        wi::renderer::SetTemporalAAEnabled(false);
        auto frame=[&]{ render.PreUpdate(); render.Update(1.f/60.f); render.PreRender(); render.Render();
            device->SubmitCommandLists(); wi::renderer::UpdateGPUSuballocator(); };
        auto capture=[&](const std::string& name){ device->WaitForGPU();
            Require(wi::helper::saveTextureToFile(render.GetRenderResult3D(),
                (fs::u8path(argv[2])/name).generic_u8string()),"capture failed"); };
        for(int i=0;i<100;++i) frame();
        terrain.Generation_Cancel();
        auto loaded=loadedHeights, warmed=CaptureTerrainSculpt(scene,terrain);
        Require(RetainChangedTerrainSculpt(loaded,warmed)==0,"warmup changed loaded authored heights");
        for(const auto& entry:terrain.chunks) if(entry.first.x==0 && entry.first.z==0){
            const auto oi=scene.objects.GetIndex(entry.second.entity);
            const auto centre=scene.aabb_objects[oi].getCenter();
            std::cout << "CENTRE=" << centre.x << ',' << centre.y << ',' << centre.z << std::endl;
            camera.TransformCamera(XMMatrixInverse(nullptr,XMMatrixLookAtLH(
                XMVectorSet(centre.x+50,centre.y+65,centre.z-90,1),XMLoadFloat3(&centre),XMVectorSet(0,1,0,0))));
            camera.UpdateCamera(); break;
        }
        for(int i=0;i<30;++i) frame();
        capture("before.png");
        // Demonstrate the previous scheduling defect without drawing the invalid
        // frame: render descriptors were already prepared when buffers changed.
        auto before=CaptureTerrainSculpt(scene,terrain);
        Require(SculptTerrain(scene,terrain,{0,0,0},12,0.1f,0.5f,TerrainSculptMode::Raise,0),"probe sculpt failed");
        size_t stale=0;
        for(const auto& entry:terrain.chunks){ auto* mesh=scene.meshes.GetComponent(entry.second.entity);
            if(mesh && !mesh->subsets.empty() && scene.geometryArrayMapped &&
                scene.geometryArrayMapped[mesh->geometryOffset].ib!=mesh->ib.descriptor_srv) ++stale; }
        std::cout << "PREVIOUS_PHASE_STALE_GEOMETRY=" << stale << std::endl;
        Require(stale>0,"late mutation did not expose stale descriptor regression");
        Require(ApplyTerrainSculpt(scene,terrain,before),"probe restore failed");
        for(int cycle=0;cycle<4;++cycle){
            terrain.Generation_Cancel();
            auto initial=CaptureTerrainSculpt(scene,terrain);
            const XMFLOAT3 brushCenter=cycle==0 ? XMFLOAT3(0,0,0) :
                cycle==1 ? XMFLOAT3(33,0,0) : cycle==2 ? XMFLOAT3(33,0,33) : XMFLOAT3(-33,0,66);
            double total=0;
            for(int step=0;step<31;++step){
                const auto start=std::chrono::steady_clock::now();
                Require(SculptTerrain(scene,terrain,brushCenter,12,.1f,.5f,
                    step%2 ? TerrainSculptMode::Lower : TerrainSculptMode::Raise,0),"repeated sculpt failed");
                frame(); total+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
                for(const auto& entry:terrain.chunks){ auto* mesh=scene.meshes.GetComponent(entry.second.entity);
                    if(mesh && !mesh->subsets.empty()) Require(
                        scene.geometryArrayMapped[mesh->geometryOffset].ib==mesh->ib.descriptor_srv,"render descriptor mismatch"); }
            }
            terrain.Generation_Cancel(); auto after=CaptureTerrainSculpt(scene,terrain);
            auto undo=initial; auto redo=after;
            Require(RetainChangedTerrainSculpt(undo,redo)>0,"stroke retained no changes");
            RefreshTerrainSculptPhysics(scene,terrain,redo);
            Require(ApplyTerrainSculpt(scene,terrain,undo),"undo failed"); frame();
            Require(ApplyTerrainSculpt(scene,terrain,redo),"redo failed"); frame();
            // The same live serialization + validation used by Test Level,
            // followed by 2D-only device frames while the editor scene is paused.
            terrain.Generation_Cancel();
            const auto snapshot=(fs::u8path(argv[2])/("snapshot-"+std::to_string(cycle)+".wiscene")).generic_u8string();
            { wi::Archive archive(snapshot,false,false); scene.Serialize(archive); Require(archive.SaveFile(snapshot),"snapshot save failed"); archive=wi::Archive(); }
            { auto validation=PrepareWickedSceneOpen(snapshot); Require(validation.IsReady(),"snapshot validation failed"); }
            fs::remove(fs::u8path(snapshot));
            for(int i=0;i<31;++i){ device->SubmitCommandLists(); wi::renderer::UpdateGPUSuballocator(); }
            for(int i=0;i<60;++i) frame();
            terrain.Generation_Cancel();
            Require(terrain.chunks.size()==chunkCount,"terrain chunk count changed");
            Require(scene.hairs.GetCount()==hairCount,"painted hair entity count changed");
            Require(Masks(scene,terrain)==loadedMasks,"authored blend/grass mask data changed");
            const auto retained=CaptureTerrainSculpt(scene,terrain);
            auto a=after,b=retained;
            Require(RetainChangedTerrainSculpt(a,b)==0,"snapshot/return changed authored heights");
            capture("return-"+std::to_string(cycle)+".png");
            std::cout << "CYCLE=" << cycle << " mean_sculpt_frame_ms=" << total/31
                << " chunks=" << chunkCount << " hairs=" << hairCount << std::endl;
        }
        std::cout << "PASS: GPU terrain lifecycle, descriptors, repeated strokes, undo/redo, snapshot returns\n";
    } catch(const std::exception& e){ std::cerr << "FAIL: " << e.what() << std::endl; return 1; }
    return 0;
}
