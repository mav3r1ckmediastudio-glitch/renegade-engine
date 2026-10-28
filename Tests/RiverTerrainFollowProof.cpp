#include "renegade/bridge/RiverSplineService.h"
#include "renegade/bridge/RiverTerrainLayerService.h"
#include "renegade/bridge/RiverbedMaterialService.h"
#include "renegade/bridge/IdentityService.h"
#include "renegade/bridge/SpecialistComponentService.h"
#include "renegade/bridge/PlayerService.h"
#include "renegade/bridge/TerrainService.h"
#include "renegade/bridge/SceneDocumentService.h"
#include "renegade/bridge/SceneService.h"
#include "renegade/bridge/ProjectService.h"
#include "renegade/bridge/FlowService.h"
#include "renegade/bridge/SelectionService.h"
#include "renegade/bridge/RiverFlowRenderer.h"

#include <Windows.h>
#include <algorithm>
#include <cmath>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>

namespace fs = std::filesystem;
using namespace renegade::bridge;

static void Require(const bool ok, const char* why)
{
    if (!ok) throw std::runtime_error(why);
}


// Rocky bed is now tested through the undoable EngineBridge command.

static int OptionalNativeLocalDepthProofShader()
{
    // Proof-only, never registered by Studio/Runtime. Native WATER normal code
    // and material remain unchanged; test A/B only the local depth expression.
    char filename[32768]{};
    const DWORD length = GetEnvironmentVariableA(
        "RENEGADE_RIVER_LOCAL_DEPTH_PROOF_PS",filename,sizeof(filename));
    if(!length) return -1;
    Require(length < sizeof(filename),"proof PS path too long");
    static wi::graphics::Shader proofPixel;
    wi::vector<std::uint8_t> contents;
    auto* device=wi::graphics::GetDevice();
    Require(wi::helper::FileRead(filename,contents) && !contents.empty(),
        "local-depth proof-only PS missing");
    Require(device->CreateShader(wi::graphics::ShaderStage::PS,
        contents.data(),contents.size(),&proofPixel),
        "local-depth proof-only PS creation failed");
    wi::renderer::CustomShader custom;
    custom.name="Diagnostic Native WATER Local Depth";
    custom.filterMask=wi::enums::FILTER_TRANSPARENT|wi::enums::FILTER_WATER;
    wi::graphics::PipelineStateDesc pipeline;
    pipeline.vs=wi::renderer::GetShader(wi::enums::VSTYPE_OBJECT_COMMON);
    pipeline.ps=&proofPixel;
    pipeline.bs=wi::renderer::GetBlendState(wi::enums::BSTYPE_TRANSPARENT);
    pipeline.dss=wi::renderer::GetDepthStencilState(wi::enums::DSSTYPE_TRANSPARENT);
    pipeline.rs=wi::renderer::GetRasterizerState(wi::enums::RSTYPE_DOUBLESIDED);
    Require(device->CreatePipelineState(&pipeline,&custom.pso[wi::enums::RENDERPASS_MAIN]),
        "local-depth diagnostic main pipeline failed");
    pipeline.vs=wi::renderer::GetShader(wi::enums::VSTYPE_SHADOW_TRANSPARENT);
    pipeline.ps=wi::renderer::GetShader(wi::enums::PSTYPE_SHADOW_TRANSPARENT);
    pipeline.bs=wi::renderer::GetBlendState(wi::enums::BSTYPE_TRANSPARENTSHADOW);
    pipeline.dss=wi::renderer::GetDepthStencilState(wi::enums::DSSTYPE_DEPTHREAD);
    pipeline.rs=wi::renderer::GetRasterizerState(wi::enums::RSTYPE_SHADOW_DOUBLESIDED);
    Require(device->CreatePipelineState(&pipeline,&custom.pso[wi::enums::RENDERPASS_SHADOW]),
        "local-depth diagnostic native shadow pipeline failed");
    return wi::renderer::RegisterCustomShader(custom);
}

static std::uint64_t GrassAndPaintDigest(const wi::terrain::Terrain& terrain)
{
    std::uint64_t hash = 14695981039346656037ull;
    const auto byte = [&](const std::uint8_t value) {
        hash = (hash ^ value) * 1099511628211ull;
    };
    for (const auto& entry : terrain.chunks)
    {
        const auto& chunk = entry.second;
        const auto* grassBytes = reinterpret_cast<const std::uint8_t*>(
            chunk.grass.vertex_lengths.data());
        for (std::size_t i = 0;
             i < chunk.grass.vertex_lengths.size() * sizeof(float); ++i)
            byte(grassBytes[i]);
        for (const auto& layer : chunk.blendmap_layers)
            for (const auto pixel : layer.pixels) byte(pixel);
        for (const auto& layer : chunk.spline_blendmap_layers)
            for (const auto pixel : layer.pixels) byte(pixel);
    }
    return hash;
}

int main(int argc, char** argv)
{
    if (argc != 2 && argc != 3) return 2;
    const auto out = fs::u8path(argv[1]);
    if (fs::exists(out) && !fs::is_empty(out))
    {
        std::cerr << "Provide a new/empty disposable directory\n";
        return 2;
    }
    fs::create_directories(out);
    HWND window = CreateWindowExW(0, L"STATIC", L"River On Actual Terrain",
        WS_OVERLAPPEDWINDOW, 0, 0, 960, 640, nullptr, nullptr,
        GetModuleHandleW(nullptr), nullptr);
    wi::Application app;
    app.allow_hdr = false;
    app.SetWindow(window);
    wi::initializer::InitializeComponentsImmediate();
    auto* gpu = wi::graphics::GetDevice();
    struct GPUDrain
    {
        ~GPUDrain()
        {
            while (wi::renderer::IsPipelineCreationActive()) Sleep(10);
            wi::graphics::GetDevice()->WaitForGPU();
        }
    } drain;
    try
    {
        if(argc==3 && std::string(argv[2])=="g13e8-review-reopen")
        {
            // Separate-process project/flow/native terrain proof; explicitly
            // rejects mismatched project registry and missing saved nodes.
            const char* home=std::getenv("USERPROFILE");
            Require(home&&home[0],"G13E8 independent reopen needs USERPROFILE");
            const auto root=fs::u8path(home)/"G13E8RiverReview_20261027"/"G13E8RiverWidthReview";
            const auto descriptor=(root/"G13E8RiverWidthReview.renegade").generic_u8string();
            ProjectService check;
            check.Initialize((out/"g13e8-independent-recents.ini").generic_u8string());
            const bool opened=check.OpenProject(descriptor);
            const auto openError=check.LastError();
            Require(opened,openError.c_str());
            const auto project=check.CurrentProject();
            std::string error;
            FlowDocument story;
            const auto path=(root/fs::u8path(project.startupFlow)).generic_u8string();
            const bool flowOk=ReadFlowDocument(path,project.projectId,story,error);
            if(!flowOk)throw std::runtime_error("G13E8 fresh flow: "+error);
            Require(story.nodes.size()>=3,"G13E8 independent Flow missing level");
            SceneService loaded;
            const auto scenePath=(root/"Content/Scenes/G13E8River.wiscene").generic_u8string();
            const bool sceneOk=loaded.LoadScene(scenePath);
            const auto sceneError=loaded.LastError();
            Require(sceneOk,sceneError.c_str());
            auto& fresh=loaded.GetScene();
            const auto river=fresh.Entity_FindByName("River Water");
            const auto* spline=fresh.splines.GetComponent(river);
            Require(spline&&spline->spline_node_entities.size()==5,
                "G13E8 fresh reopen lacks native spline");
            const auto node=spline->spline_node_entities[1];
            Require(std::abs(fresh.transforms.GetComponent(node)->scale_local.x-1.6f)<.0001f,
                "G13E8 fresh project lost per-node width");
            RiverFlowProfile profile;
            const bool settings=ReadRiverFlowProfile(fresh,river,profile,error);
            if(!settings)throw std::runtime_error("G13E8 fresh river profile: "+error);
            Require(profile.enabled&&profile.startWhitewater&&profile.startWhitewaterAmount==.65f,
                "G13E8 fresh project lost calibrated Start Whitewater");
            Require(fresh.terrains.GetCount()==1,
                "G13E8 fresh project lost authored linked terrain");
            std::cout<<"G13E8_INDEPENDENT_PROJECT_FLOW_RIVER_TERRAIN_SCALE_FOAM_REOPEN_PASS"
              <<" nodes="<<spline->spline_node_entities.size()
              <<" startFoam="<<profile.startWhitewater
              <<" amount="<<profile.startWhitewaterAmount<<std::endl;
            return 0;
        }
        wi::scene::Scene scene;
        if(argc==3 && std::string(argv[2])=="rocky-reopen")
        {
            const auto stored=std::getenv("RENEGADE_ROCKY_SAVED_WISCENE");
            Require(stored && stored[0],"rocky reopen WISCENE path missing");
            // Exercise the actual Runtime scene-adoption service, including
            // resource hydration, rather than a stripped archive-only loader.
            SceneService adopted;
            Require(adopted.LoadScene(stored),adopted.LastError().c_str());
            scene.Merge(adopted.GetScene());
            Require(scene.terrains.GetCount()==1,
                "reopened rocky scene must have one terrain");
            auto& t=scene.terrains[0];
            t.scene=&scene;
            t.terrainEntity=scene.terrains.GetEntity(0);
            Require(t.materialEntities.size()==5,
                "reopened scene did not preserve fifth rocky material slot");
            // A real second process cannot inherit Studio's GPU resources:
            // explicitly restore the bundled fifth-slot PBR maps from its
            // own executable's Content directory and retile legacy 0.25x.
            std::string rebindError;
            Require(RebindBundledRockyRiverbedMaterials(scene,rebindError),
                "bundled rocky material could not hydrate on fresh reopen");
            const auto* rocks=scene.materials.GetComponent(t.materialEntities.back());
            Require(rocks &&
                rocks->textures[wi::scene::MaterialComponent::BASECOLORMAP]
                    .resource.GetTexture().IsValid() &&
                rocks->textures[wi::scene::MaterialComponent::NORMALMAP]
                    .resource.GetTexture().IsValid() &&
                rocks->textures[wi::scene::MaterialComponent::SURFACEMAP]
                    .resource.GetTexture().IsValid(),
                "native terrain rocky maps are not GPU-resident after reopen");
            Require(std::abs(rocks->texMulAdd.x-1.f)<.001f,
                "reopen failed to correct prototype boulder-scale tiling");
            std::cout<<"ROCKY_RIVERBED_FRESH_GPU_TEXTURE_BINDING=PASS "
                <<"tileX="<<rocks->texMulAdd.x<<"\n";
            auto masksValid=[&]()
            {
                for(const auto& kv:t.chunks)
                    if(kv.second.blendmap_layers.size()!=5)return false;
                return !t.chunks.empty();
            };
            Require(masksValid(),"native rocky scene missing fifth saved mask");
            // The exact archived paint bytes, including existing 4 material
            // layers, MUST survive the subsequent native chunk generator.
            // Regression against runtime-only white material/bank corruption.
            const auto layerHash=[&](const size_t maxLayers)
            {
                std::uint64_t hash=14695981039346656037ull;
                for(const auto& kv:t.chunks)
                    for(size_t i=0;i<std::min(
                        maxLayers,kv.second.blendmap_layers.size());++i)
                        for(const auto v:kv.second.blendmap_layers[i].pixels)
                            hash=(hash^v)*1099511628211ull;
                return hash;
            };
            const auto basePaintHashBefore=layerHash(4);
            const auto allPaintHashBefore=layerHash(5);
            const auto authoredSculptBefore=CaptureTerrainSculpt(scene,t);
            wi::scene::CameraComponent c;
            c.CreatePerspective(960,640,.1f,500.f,
                DirectX::XMConvertToRadians(60));
            c.TransformCamera(DirectX::XMMatrixInverse(nullptr,
                DirectX::XMMatrixLookAtLH(
                    DirectX::XMVectorSet(8,55,-33,1),
                    DirectX::XMVectorSet(0,29,0,1),
                    DirectX::XMVectorSet(0,1,0,0))));
            c.UpdateCamera();
            wi::RenderPath3D p;
            p.scene=&scene;p.camera=&c;p.init(960,640,96);
            p.setOcclusionCullingEnabled(false);
            p.setMotionBlurEnabled(false);
            p.setDepthOfFieldEnabled(false);
            p.ResizeBuffers();
            wi::renderer::SetTemporalAAEnabled(false);
            app.ActivatePath(&p);
            for(int i=0;i<120;++i){app.Run();if(i%24==0)Sleep(10);}
            for(int i=0;i<120 &&
                wi::renderer::IsPipelineCreationActive();++i)Sleep(25);
            gpu->WaitForGPU();
            // Detect native generation invalidating/removing the extra mask:
            if(!masksValid())
                throw std::runtime_error(
                    "native terrain regeneration erased rocky material masks");
            Require(t.materialEntities.size()==5,
                "reopened render removed fifth rocky material");
            std::cout<<"TERRAIN_REGEN_PAINT_CHECK baseOld="
                <<basePaintHashBefore<<" baseNew="<<layerHash(4)
                <<" allOld="<<allPaintHashBefore
                <<" allNew="<<layerHash(5)<<"\n";
            Require(layerHash(4)==basePaintHashBefore,
                "regeneration modified previously authored grass/material paint");
            Require(layerHash(5)==allPaintHashBefore,
                "regeneration modified fifth or existing painted terrain masks");
            std::cout<<"ROCKY_RIVERBED_ALL_FIVE_MASKS_BIT_IDENTICAL_AFTER_REGEN=PASS\n";
            auto expectedSculpt=authoredSculptBefore;
            auto actualSculpt=CaptureTerrainSculpt(scene,t);
            Require(RetainChangedTerrainSculpt(expectedSculpt,actualSculpt)==0,
                "fresh-process rendering destroyed authored terrain sculpt");
            std::cout<<"ROCKY_RIVERBED_REOPEN_AUTHORED_SCULPT_UNCHANGED=PASS\n";
            const auto frame=(out/"rocky-reopened-native-dx12.png").generic_u8string();
            Require(wi::helper::saveTextureToFile(
                p.GetRenderResult3D(),frame),
                "reopened rocky WISCENE DX12 image could not save");
            if(const auto runtimeScene=std::getenv("RENEGADE_ROCKY_RUNTIME_EXPORT"))
            {
                // A graphics fixture's external camera is not serialized.
                // Give the disposable Runtime scene an explicit playable start.
                const auto hit=wi::scene::Pick(wi::primitive::Ray(
                    XMFLOAT3(-18,150,-5),XMFLOAT3(0,-1,0),0,300),
                    wi::enums::FILTER_TERRAIN,~0u,scene);
                Require(hit.entity!=wi::ecs::INVALID_ENTITY,
                    "runtime diagnostic start missed solid terrain");
                TransformState spawn;
                spawn.translation={-18,hit.position.y+1.1f,-5};
                DirectX::XMStoreFloat4(&spawn.rotation,
                    DirectX::XMQuaternionRotationRollPitchYaw(0,DirectX::XM_PIDIV2,0));
                const auto existingStart=ResolvePlayerStart(scene);
                if(existingStart.resolution==PlayerStartResolution::Missing)
                {
                    CreatePlayerStartCommand start(scene,spawn);
                    Require(start.Execute(),"runtime diagnostic Player Start failed");
                }
                else
                {
                    Require(existingStart.resolution==PlayerStartResolution::Success,
                        "runtime fixture has ambiguous Player Starts");
                    auto* transform=scene.transforms.GetComponent(existingStart.start.entity);
                    Require(transform!=nullptr,"runtime fixture start transform missing");
                    transform->translation_local=spawn.translation;
                    transform->rotation_local=spawn.rotation;
                    transform->SetDirty();
                    transform->UpdateTransform();
                }
                t.SetPhysicsEnabled(true);
                t.physics_generation=3;
                wi::Archive exported(runtimeScene,false,false);
                scene.Serialize(exported);
                Require(exported.SaveFile(runtimeScene),"runtime fixture export failed");
                std::cout<<"ROCKY_RUNTIME_EXPLICIT_PLAYER_START=PASS groundY="
                    <<hit.position.y<<"\n";
            }
            app.ActivatePath(nullptr);
            std::cout<<"ROCKY_RIVERBED_FRESH_PROCESS_GPU_REOPEN=PASS"
                <<" chunks="<<t.chunks.size()
                <<" blendmap_layers="<<t.chunks.begin()->second.blendmap_layers.size()
                <<" native_materials="<<t.materialEntities.size()<<"\n";
            return 0;
        }
        auto& weather = scene.weathers.Create(
            scene.Entity_CreateTransform("Proof Weather"));
        weather.ambient = {0.75f, 0.75f, 0.75f};
        weather.horizon = {0.58f, 0.66f, 0.74f};
        weather.zenith = {0.29f, 0.47f, 0.69f};
        scene.Entity_CreateLight("Proof Light", {15.0f, 60.0f, -20.0f},
            {1.0f, 1.0f, 0.96f}, 220.0f, 200.0f,
            wi::scene::LightComponent::POINT);
        TerrainState settings;
        settings.visibleChunkRadius = 1;
        settings.propChunkRadius = 0;
        settings.physicsChunkRadius = 0;
        settings.physics = false;
        settings.minimumHeight = -20;
        settings.maximumHeight = 40;
        const auto terrainID = CreateTerrain(scene, settings, "Disposable Real Terrain");
        Require(terrainID != wi::ecs::INVALID_ENTITY,
            "native terrain creation failed");
        auto* terrain = scene.terrains.GetComponent(terrainID);
        Require(terrain != nullptr, "native terrain component missing");
        for (std::size_t i=0;i<scene.materials.GetCount();++i)
            for (const auto slot : {wi::scene::MaterialComponent::BASECOLORMAP,
                wi::scene::MaterialComponent::NORMALMAP,wi::scene::MaterialComponent::SURFACEMAP})
            {
                const auto& texture=scene.materials[i].textures[slot];
                if (!texture.name.empty())
                    Require(texture.resource.IsValid() && texture.resource.GetTexture().IsValid(),
                        "native terrain material texture is missing from GPU proof deployment");
            }
        std::cout << "NATIVE_TERRAIN_MATERIAL_RESOURCES=PASS\n";
        wi::scene::CameraComponent camera;
        camera.CreatePerspective(960,640,0.1f,500.0f,
            DirectX::XMConvertToRadians(60.0f));
        camera.TransformCamera(DirectX::XMMatrixInverse(nullptr,
            DirectX::XMMatrixLookAtLH(
                DirectX::XMVectorSet(80,100,-100,1),
                DirectX::XMVectorSet(0,0,0,1),
                DirectX::XMVectorSet(0,1,0,0))));
        camera.UpdateCamera();
        wi::RenderPath3D render;
        render.scene = &scene;
        render.camera = &camera;
        render.init(960,640,96);
        render.setOcclusionCullingEnabled(false);
        render.setMotionBlurEnabled(false);
        render.setDepthOfFieldEnabled(false);
        render.ResizeBuffers();
        wi::renderer::SetTemporalAAEnabled(false);
        app.ActivatePath(&render);
        struct ScopedDisarm
        {
            wi::Application& app;
            ~ScopedDisarm() { app.ActivatePath(nullptr); }
        } disarm{app};
        for (int frame=0;frame<160;++frame)
        {
            app.Run();
            if(frame%24==0) Sleep(10);
        }
        for(int ms=0;ms<15000 && wi::renderer::IsPipelineCreationActive();ms+=25)
            Sleep(25);
        for(int frame=0;frame<20;++frame) app.Run();
        terrain->Generation_Cancel();
        // Never rewrite source chunk buffers while a previous submitted GPU
        // frame or the generator can still own them.
        gpu->WaitForGPU();
        Require(terrain->chunks.size() >= 9,
            "real finite terrain did not generate all chunks");
        const auto chunksBefore = terrain->chunks.size();
        std::cout << "TERRAIN_CHUNKS=" << chunksBefore << "\n";


        if(argc==3 && (std::string(argv[2])=="flat-nodescale"||
                        std::string(argv[2])=="flat-nodescale-review")){
            // G13E8 real native 3x3 terrain, not a synthetic or unsupported
            // elevated course. The linked riverbed must reshape with native
            // per-node WATER widths, while preserving the original 4 paint masks.
            RiverSplineOptions opts;opts.widthMeters=8.f;
            opts.surfaceOffsetMeters=-.15f;
            CreateRiverSplineCommand create(scene,
                {{-20,0,0},{-8,0,0},{0,0,0},{12,0,0},{20,0,0}},opts);
            Require(create.Execute(),"G13E8 flat WATER river creation failed");
            for(int k=0;k<18;++k)app.Run();
            gpu->WaitForGPU();terrain->Generation_Cancel();
            const auto river=create.CreatedEntity(),paintBefore=GrassAndPaintDigest(*terrain);
            // Identical headroom transaction to existing flat-rocky proof:
            // generated terrain may already sit on the encoded lower limit.
            SetTerrainHeightFloorCommand floor(scene,terrainID,terrain->bottomLevel-10.f);
            Require(floor.Execute(),"G13E8 flat native terrain floor headroom refused");
            SetRiverTerrainLayerCommand attach(scene,terrainID,river,1.35f,2.f,true);
            const bool linked=attach.Execute();
            if(!linked)throw std::runtime_error(std::string("G13E8 link rejected: ")+attach.Error());
            const auto spline=scene.splines.GetComponent(river);
            Require(spline&&spline->spline_node_entities.size()==5,"G13E8 river nodes missing");
            const auto selected=spline->spline_node_entities[1];
            const auto before=CaptureTransform(*scene.transforms.GetComponent(selected));
            auto after=before;after.scale.x=1.6f;
            auto terrainBefore=CaptureTerrainSculpt(scene,*terrain);
            CommandService history;
            Require(history.Execute(std::make_unique<SetWatercourseTransformCommand>(
                scene,selected,before,after)),"G13E8 linked Scale X transaction refused");
            scene.Update(0);
            auto terrainAfter=CaptureTerrainSculpt(scene,*terrain);
            auto a=terrainBefore,b=terrainAfter;
            Require(RetainChangedTerrainSculpt(a,b)>0,
                "G13E8 widened river failed to update native carved terrain");
            Require(std::abs(scene.transforms.GetComponent(selected)->scale_local.x-1.6f)<.001f,
                "G13E8 native node width lost");
            camera.TransformCamera(DirectX::XMMatrixInverse(nullptr,
                DirectX::XMMatrixLookAtLH(DirectX::XMVectorSet(4,32,-35,1),
                    DirectX::XMVectorSet(-5,0,0,1),DirectX::XMVectorSet(0,1,0,0))));
            camera.UpdateCamera();
            for(int i=0;i<25;++i)app.Run();gpu->WaitForGPU();
            Require(wi::helper::saveTextureToFile(render.GetRenderResult3D(),
                (out/"g13e8-scaled-node-river.png").generic_u8string()),
                "G13E8 native Scale X screenshot failed");
            Require(history.Undo(),"G13E8 river node scale Undo refused");
            auto u0=terrainBefore,u1=CaptureTerrainSculpt(scene,*terrain);
            Require(RetainChangedTerrainSculpt(u0,u1)==0 &&
                std::abs(scene.transforms.GetComponent(selected)->scale_local.x-1.f)<.0001f,
                "G13E8 native Undo lost bank or selected width");
            Require(history.Redo(),"G13E8 river node scale Redo refused");
            auto r0=terrainAfter,r1=CaptureTerrainSculpt(scene,*terrain);
            Require(RetainChangedTerrainSculpt(r0,r1)==0 &&
                std::abs(scene.transforms.GetComponent(selected)->scale_local.x-1.6f)<.0001f,
                "G13E8 native Redo lost bank or selected width");
            // Regression for owner's exact report: the width must have MANY
            // intermediate values and return to its original width without
            // relying on Undo.
            auto restored=after;restored.scale.x=1.f;
            auto narrower=after;narrower.scale.x=.7f;
            Require(history.Execute(std::make_unique<SetWatercourseTransformCommand>(
                scene,selected,after,restored)),
                "G13E8 width could not return to its original size without Undo");
            auto originalTerrain=CaptureTerrainSculpt(scene,*terrain);
            auto originalA=terrainBefore,originalB=originalTerrain;
            Require(RetainChangedTerrainSculpt(originalA,originalB)==0,
                "G13E8 reset width did not restore original terrain banks");
            Require(history.Execute(std::make_unique<SetWatercourseTransformCommand>(
                scene,selected,restored,narrower)),
                "G13E8 width .7x intermediate shrink refused");
            Require(std::abs(scene.transforms.GetComponent(selected)->scale_local.x-.7f)<.0001f,
                "G13E8 arbitrary intermediate width was not applied");
            Require(history.Execute(std::make_unique<SetWatercourseTransformCommand>(
                scene,selected,narrower,after)),
                "G13E8 widen again after shrinking without Undo refused");
            auto returnedA=terrainAfter,returnedB=CaptureTerrainSculpt(scene,*terrain);
            Require(RetainChangedTerrainSculpt(returnedA,returnedB)==0,
                "G13E8 shrink-and-widen cycle corrupted linked bank");
            std::cout<<"G13E8_CONTINUOUS_WIDTH_RESET_ROUNDTRIP_PASS"
                <<" x=1.6->1.0->0.7->1.6 without Undo"<<std::endl;
            auto invalid=after;invalid.scale.x=10.f;
            SetWatercourseTransformCommand reject(scene,selected,after,invalid);
            Require(!reject.Execute(),"G13E8 invalid node width was accepted");
            auto p0=terrainAfter,p1=CaptureTerrainSculpt(scene,*terrain);
            Require(RetainChangedTerrainSculpt(p0,p1)==0,
                "G13E8 invalid width edit changed ground");
            const auto filename=(out/"g13e8-scaled-native.wiscene").generic_u8string();
            wi::Archive disk(filename,false,false);scene.Serialize(disk);
            Require(disk.SaveFile(filename),"G13E8 native scaled river save failed");
            wi::scene::Scene opened;
            wi::Archive file(filename,true,false);opened.Serialize(file);
            const auto riverSaved=opened.Entity_FindByName("River Water");
            const auto* riverSpline=opened.splines.GetComponent(riverSaved);
            Require(riverSpline&&riverSpline->spline_node_entities.size()==5,
                "G13E8 native river missing after disk reopen");
            Require(std::abs(opened.transforms.GetComponent(
                riverSpline->spline_node_entities[1])->scale_local.x-1.6f)<.0001f,
                "G13E8 native scale lost on disk reopen");
            Require(GrassAndPaintDigest(*terrain)==paintBefore,
                "G13E8 river node width modified grass or unrelated materials");
            if(std::string(argv[2])=="flat-nodescale-review"){
            // Produce a disposable REAL native Studio project using the same
            // linked bed + variable-width authoring proven above. The imported
            // archive is separate from the proof scene and from owner projects.
            std::cout<<"G13E8_REVIEW_STAGE=proof_passed_begin_clone"<<std::endl;
            SceneService adopted;
            const bool adoptedOk=adopted.LoadScene(filename);
            const auto adoptedError=adopted.LastError();
            Require(adoptedOk,adoptedError.c_str());
            std::cout<<"G13E8_REVIEW_STAGE=adopted"<<std::endl;
            auto& review=adopted.GetScene();
            const auto reviewRiver=review.Entity_FindByName("River Water");
            Require(review.splines.Contains(reviewRiver),"G13E8 review river absent");
            std::string error;
            const bool identified=EnsurePersistentEntityIdentities(review,error);
            if(!identified)throw std::runtime_error("G13E8 IDs: "+error);
            std::cout<<"G13E8_REVIEW_STAGE=identities"<<std::endl;
            RiverFlowProfile flow;
            const bool profileOk=ReadRiverFlowProfile(review,reviewRiver,flow,error);
            if(!profileOk)throw std::runtime_error("G13E8 profile: "+error);
            flow.enabled=true;flow.foam=true;flow.startWhitewater=true;
            flow.startWhitewaterAmount=.65f;flow.speed=1.0f;
            SetRiverFlowProfileCommand enableSource(review,reviewRiver,flow);
            Require(enableSource.Execute(),"G13E8 review source settings failed");
            auto sceneCamera=review.Entity_CreateCamera("G13E8 River Review Camera",
                1264,761,.05f,1000,DirectX::XMConvertToRadians(60));
            auto* viewPose=review.transforms.GetComponent(sceneCamera);
            viewPose->MatrixTransform(DirectX::XMMatrixInverse(nullptr,
                DirectX::XMMatrixLookAtLH(
                    DirectX::XMVectorSet(3,18,-25,1),
                    DirectX::XMVectorSet(-8,0,0,1),
                    DirectX::XMVectorSet(0,1,0,0))));
            viewPose->UpdateTransform();
            review.cameras.GetComponent(sceneCamera)->TransformCamera(*viewPose);
            TransformState spawn;
            spawn.translation={-16.f,1.f,-7.f};
            CreatePlayerStartCommand player(review,spawn);
            Require(player.Execute(),"G13E8 review player start missing");
            std::cout<<"G13E8_REVIEW_STAGE=player_and_profile"<<std::endl;
            // The Windows document journal adds 70+ path characters and
            // fails at MAX_PATH when nested in BUILD/evidence. Create this
            // owner-review disposable under the user's profile instead.
            const char* home=std::getenv("USERPROFILE");
            Require(home&&home[0],"G13E8 review requires USERPROFILE");
            const auto reviewParent=fs::u8path(home)/"G13E8RiverReview_20261027";
            Require(!fs::exists(reviewParent),
                "Refusing to overwrite an existing owner review project");
            fs::create_directories(reviewParent);
            ProjectService projects;
            projects.Initialize((reviewParent/"recents.ini").generic_u8string());
            const bool projectCreated=projects.CreateStoryFlowProject(
                reviewParent.generic_u8string(),"G13E8RiverWidthReview",false);
            const auto projectError=projects.LastError();
            Require(projectCreated,projectError.c_str());
            std::cout<<"G13E8_REVIEW_STAGE=project_created"<<std::endl;
            const auto project=projects.CurrentProject();
            const auto reviewPath=fs::u8path(project.rootPath)/
                "Content/Scenes/G13E8River.wiscene";
            SelectionService reviewSelection;
            CommandService reviewHistory;
            SceneDocumentService documents(adopted,reviewSelection,reviewHistory,projects);
            const bool saved=documents.Save(reviewPath.generic_u8string());
            const auto saveError=adopted.LastError();
            Require(saved,saveError.c_str());
            std::cout<<"G13E8_REVIEW_STAGE=scene_saved"<<std::endl;
            const auto document=CreateDocumentEnvelope(project.projectId,
                SceneDocumentType,"Content/Scenes/G13E8River.wiscene",
                "g13e8-width-foam-review");
            if(!WriteDocumentEnvelope(reviewPath.generic_u8string()+".rmeta",
                document,error))throw std::runtime_error("G13E8 envelope: "+error);
            std::cout<<"G13E8_REVIEW_STAGE=scene_envelope"<<std::endl;
            FlowDocument story;
            const auto flowPath=(fs::u8path(project.rootPath)/
                fs::u8path(project.startupFlow)).generic_u8string();
            if(!ReadFlowDocument(flowPath,project.projectId,story,error))
                throw std::runtime_error("G13E8 flow load: "+error);
            FlowNode level;level.id=GenerateStableId();
            level.kind=FlowNodeKind::Level;level.name="G13E8 River Width and Whitewater";
            level.sceneAssetId=document.documentId;
            level.scenePathHint="Content/Scenes/G13E8River.wiscene";
            FlowNode complete;complete.id=GenerateStableId();
            complete.kind=FlowNodeKind::CompleteGame;complete.name="Complete Game";
            FlowRoute entry;entry.id=GenerateStableId();
            entry.sourceNodeId=story.startNodeId;entry.outcome=GameStartOutcome;
            entry.destinationNodeId=level.id;entry.destinationEntry="default";
            FlowRoute exit;exit.id=GenerateStableId();
            exit.sourceNodeId=level.id;exit.outcome="level.complete";
            exit.destinationNodeId=complete.id;
            story.nodes.push_back(level);story.nodes.push_back(complete);
            story.routes.push_back(entry);story.routes.push_back(exit);
            if(!WriteFlowDocument(flowPath,story,error))
                throw std::runtime_error("G13E8 flow write: "+error);
            std::cout<<"G13E8_REVIEW_STAGE=flow_saved"<<std::endl;
            std::cout<<"G13E8_STUDIO_REVIEW_PROJECT="<<project.descriptorPath
                <<" SOURCE_FOAM=ON NODE_WIDTH_X=1.6"<<std::endl;
            }
            std::cout<<"G13E8_FLAT_NODE_WIDTH_NATIVE_LINKED_BANK_UNDO_REDO_REOPEN_PASS"<<std::endl;
            return 0;
        }

        if(argc==3 && std::string(argv[2])=="flat-rocky")
        {
            RiverSplineOptions options;
            options.widthMeters=8;
            CreateRiverSplineCommand create(scene,{{-20,0,0},{0,0,0},{20,0,0}},options);
            Require(create.Execute(),"flat river creation failed");
            for(int i=0;i<20;++i)app.Run();
            gpu->WaitForGPU();terrain->Generation_Cancel();
            const auto river=create.CreatedEntity();
            std::string error;
            Require(EnsurePersistentEntityIdentities(scene,error),error.c_str());
            SetTerrainHeightFloorCommand floor(scene,terrainID,terrain->bottomLevel-10.f);
            Require(floor.Execute(),"flat river deeper-bed floor failed");
            SetRiverTerrainLayerCommand link(scene,terrainID,river,1.35f,2.f,true);
            const bool linked=link.Execute();
            Require(linked,link.Error().c_str());
            const auto sculptBefore=CaptureTerrainSculpt(scene,*terrain);
            const auto paintBefore=GrassAndPaintDigest(*terrain);
            PaintRockyRiverbedCommand paint(scene,terrainID,river,
                fs::absolute("Content/terrain/riverbed/rocky_rugged").generic_u8string());
            const bool painted=paint.Execute();
            Require(painted,paint.Error().c_str());
            Require(terrain->materialEntities.size()==5,"flat rocky layer missing");
            for(int i=0;i<80;++i)app.Run();
            gpu->WaitForGPU();
            camera.TransformCamera(DirectX::XMMatrixInverse(nullptr,
                DirectX::XMMatrixLookAtLH(DirectX::XMVectorSet(10,28,-32,1),
                    DirectX::XMVectorSet(0,0,0,1),DirectX::XMVectorSet(0,1,0,0))));
            camera.UpdateCamera();
            for(int i=0;i<20;++i)app.Run();
            gpu->WaitForGPU();
            Require(wi::helper::saveTextureToFile(render.GetRenderResult3D(),
                (out/"flat-rocky.png").generic_u8string()),"flat rocky capture failed");
            auto a=sculptBefore,b=CaptureTerrainSculpt(scene,*terrain);
            Require(RetainChangedTerrainSculpt(a,b)==0,"flat paint changed sculpt");
            paint.Undo();
            Require(GrassAndPaintDigest(*terrain)==paintBefore,
                "flat rocky Undo changed original grass/paint");
            Require(paint.Execute(),"flat rocky Redo failed");
            const auto firstRiverPaint=GrassAndPaintDigest(*terrain);
            const auto sharedMaterial=terrain->materialEntities.back();
            CreateRiverSplineCommand second(scene,{{0,0,-22},{0,0,0},{0,0,22}},options);
            Require(second.Execute(),"second flat river creation failed");
            for(int i=0;i<20;++i)app.Run();
            gpu->WaitForGPU();terrain->Generation_Cancel();
            Require(EnsurePersistentEntityIdentities(scene,error),error.c_str());
            SetRiverTerrainLayerCommand linkSecond(scene,terrainID,second.CreatedEntity(),1.35f,2.f,true);
            const bool linkedSecond=linkSecond.Execute();
            Require(linkedSecond,linkSecond.Error().c_str());
            const auto sharedSculpt=CaptureTerrainSculpt(scene,*terrain);
            PaintRockyRiverbedCommand paintSecond(scene,terrainID,second.CreatedEntity(),
                fs::absolute("Content/terrain/riverbed/rocky_rugged").generic_u8string());
            const bool paintedSecond=paintSecond.Execute();
            Require(paintedSecond,paintSecond.Error().c_str());
            Require(terrain->materialEntities.size()==5 &&
                terrain->materialEntities.back()==sharedMaterial,"second river duplicated material");
            const auto combinedPaint=GrassAndPaintDigest(*terrain);
            Require(combinedPaint!=firstRiverPaint,"second river added no paint");
            auto expected=sharedSculpt,actual=CaptureTerrainSculpt(scene,*terrain);
            Require(RetainChangedTerrainSculpt(expected,actual)==0,"second paint changed sculpt");
            paintSecond.Undo();
            Require(GrassAndPaintDigest(*terrain)==firstRiverPaint,
                "second river Undo damaged first river paint");
            Require(paintSecond.Execute(),"second river Redo failed");
            Require(GrassAndPaintDigest(*terrain)==combinedPaint,"combined paint Redo mismatch");
            paintSecond.Undo();paint.Undo();
            Require(GrassAndPaintDigest(*terrain)==paintBefore,"two paint Undos lost original grass");
            Require(paint.Execute() && paintSecond.Execute(),"two paint Redos failed");
            Require(GrassAndPaintDigest(*terrain)==combinedPaint,"two paint Redos lost combined masks");
            for(int i=0;i<60;++i)app.Run();
            gpu->WaitForGPU();
            Require(wi::helper::saveTextureToFile(render.GetRenderResult3D(),
                (out/"two-flat-rocky.png").generic_u8string()),"two river capture failed");
            std::cout<<"MULTI_RIVER_SHARED_MATERIAL_OVERLAP_UNDO_REDO=PASS\n";
            const auto saved=(out/"flat-rocky.wiscene").generic_u8string();
            wi::Archive disk(saved,false,false);scene.Serialize(disk);
            Require(disk.SaveFile(saved),"flat rocky save failed");
            std::cout<<"FLAT_ROCKY_LINK_PAINT_SCULPT_UNDO_REDO_SAVE=PASS\n";
            return 0;
        }
        if(argc==3 && std::string(argv[2]).find("mountain")==0)
        {
            auto sculpt=CaptureTerrainSculpt(scene,*terrain);
            for(auto& chunk:sculpt.chunks)
            {
                const auto* mesh=scene.meshes.GetComponent(chunk.entity);
                const auto world=scene.ComputeEntityMatrixRecursive(chunk.entity);
                const auto inverse=DirectX::XMMatrixInverse(nullptr,world);
                for(std::size_t i=0;i<chunk.heights.size();++i)
                {
                    XMFLOAT3 p;
                    DirectX::XMStoreFloat3(&p,DirectX::XMVector3TransformCoord(
                        DirectX::XMLoadFloat3(&mesh->vertex_positions[i]),world));
                    p.y=30.f-14.f*std::tanh(p.x/25.f)+5.f*std::tanh(p.z/10.f)
                        +1.1f*std::exp(-p.x*p.x/12.f);
                    XMFLOAT3 local;
                    DirectX::XMStoreFloat3(&local,DirectX::XMVector3TransformCoord(
                        DirectX::XMLoadFloat3(&p),inverse));
                    chunk.heights[i]=local.y;
                }
            }
            terrain->SetPhysicsEnabled(true);terrain->physics_generation=2;
            wi::physics::SetEnabled(true);wi::physics::SetSimulationEnabled(false);
            Require(ApplyTerrainSculpt(scene,*terrain,sculpt),"mountain sculpt apply failed");
            for(int i=0;i<20;++i)app.Run();
            gpu->WaitForGPU();terrain->Generation_Cancel();
            const auto masks=GrassAndPaintDigest(*terrain);
            auto pick=[&](float x,float z) {
                const auto hit=wi::scene::Pick(wi::primitive::Ray(XMFLOAT3(x,150,z),XMFLOAT3(0,-1,0),0,300),
                    wi::enums::FILTER_TERRAIN,~0u,scene);
                Require(hit.entity!=wi::ecs::INVALID_ENTITY,"mountain terrain pick missed");
                return hit.position.y;
            };
            const std::string variant=argv[2];
            const bool shortReach=variant=="mountain-short";
            const bool straight=variant=="mountain-straight";
            const bool crossing=variant=="mountain-cross";
            std::vector<XMFLOAT3> points;
            const int first=crossing?20:shortReach?-6:-22;
            const int end=crossing?48:shortReach?6:22;
            const int step=shortReach?12:4;
            for(int x=first;x<=end;x+=step)
            {
                const float z=straight?0.f:2.f*std::sin(float(x)/12.f);
                points.push_back({float(x),pick(float(x),z),z});
            }
            RiverSplineOptions options;options.widthMeters=variant=="mountain-subgrid"?2.f:variant=="mountain-narrow"?3.f:4.f;
            CreateRiverSplineCommand create(scene,points,options);
            Require(create.Execute(),"mountain river create failed");
            const auto river=create.CreatedEntity();
            auto poseCamera=[&](XMFLOAT3 eye,XMFLOAT3 target) {
                camera.TransformCamera(DirectX::XMMatrixInverse(nullptr,
                    DirectX::XMMatrixLookAtLH(DirectX::XMLoadFloat3(&eye),
                        DirectX::XMLoadFloat3(&target),DirectX::XMVectorSet(0,1,0,0))));
                camera.UpdateCamera();
                for(int i=0;i<22;++i)app.Run();
                gpu->WaitForGPU();
            };
            auto capture=[&](const char* filename) {
                Require(wi::helper::saveTextureToFile(render.GetRenderResult3D(),
                    (out/filename).generic_u8string()),"mountain GPU capture failed");
            };
            poseCamera({8,55,-33},{0,29,0});capture("mountain-before.png");
            MountainStreamFit planned;std::string error;
            const auto initial=CaptureTerrainSculpt(scene,*terrain);
            const auto planStart=std::chrono::steady_clock::now();
            const bool planOK=PlanMountainStream(scene,terrainID,river,planned,error);
            std::cout<<"MOUNTAIN_PLAN_CPU_MS="<<std::chrono::duration<double,std::milli>(
                std::chrono::steady_clock::now()-planStart).count()<<" variant="<<variant<<"\n";
            Require(planOK,error.c_str());
            auto unmodified=CaptureTerrainSculpt(scene,*terrain),original=initial;
            Require(RetainChangedTerrainSculpt(original,unmodified)==0,"fit planning changed terrain");
            std::cout<<"MOUNTAIN_PLAN points="<<planned.points.size()
                <<" length="<<planned.lengthMeters<<" maxCut="<<planned.maximumCutMeters<<"\n";
            CommandService history;
            auto command=std::make_unique<FitMountainStreamCommand>(scene,terrainID,river,1.35f,2.f);
            auto* fitCommand=command.get();
            const bool fitted=fitCommand->Execute();
            const auto fitError=fitCommand->Error();
            if(variant=="mountain-subgrid")
            {
                Require(!fitted,"subgrid channel should fail closed");
                auto before=initial,after=CaptureTerrainSculpt(scene,*terrain);
                Require(RetainChangedTerrainSculpt(before,after)==0 &&
                    GrassAndPaintDigest(*terrain)==masks,
                    "subgrid refusal altered sculpt or masks");
                const auto* originalSpline=scene.splines.GetComponent(river);
                Require(originalSpline->spline_node_entities.size()==points.size(),
                    "subgrid refusal leaked generated control points");
                for(std::size_t i=0;i<points.size();++i)
                {
                    const auto* node=scene.transforms.GetComponent(originalSpline->spline_node_entities[i]);
                    Require(std::abs(node->translation_local.y-(points[i].y-.15f))<.0001f,
                        "subgrid refusal failed to restore authored control points");
                }
                std::cout<<"MOUNTAIN_SUBGRID_REFUSAL_ROLLBACK=PASS reason="<<fitError<<"\n";
                return 0;
            }
            Require(fitted,fitError.c_str());
            Require(history.RecordExecuted(std::move(command)),"mountain history refused");
            poseCamera({8,55,-33},{0,29,0});capture("mountain-fitted.png");
            if(variant=="mountain-nodeedit")
            {
                const auto nodes=scene.splines.GetComponent(river)->spline_node_entities;
                std::vector<TransformState> originalNodes;
                for(auto node:nodes)originalNodes.push_back(CaptureTransform(*scene.transforms.GetComponent(node)));
                const auto selected=nodes.size()/2;
                const auto before=originalNodes[selected];
                auto after=before;after.translation.z+=6.f;
                const auto terrainBefore=CaptureTerrainSculpt(scene,*terrain);
                auto quality=[&]()
                {
                    const auto& mesh=*scene.meshes.GetComponent(river);
                    const auto centres=SampleSplineWorldPoints(scene,river,mesh.vertex_positions.size()/2);
                    const auto world=scene.ComputeEntityMatrixRecursive(river);
                    float error=0;size_t degenerate=0;
                    for(size_t i=0;i<centres.size();++i)
                    {
                        const auto& a=mesh.vertex_positions[i*2];
                        const auto& b=mesh.vertex_positions[i*2+1];
                        XMFLOAT3 actual;
                        DirectX::XMStoreFloat3(&actual,DirectX::XMVector3TransformCoord(
                            DirectX::XMVectorSet((a.x+b.x)*.5f,(a.y+b.y)*.5f,(a.z+b.z)*.5f,1),world));
                        const auto d=DirectX::XMLoadFloat3(&actual)-DirectX::XMLoadFloat3(&centres[i]);
                        error=std::max(error,DirectX::XMVectorGetX(DirectX::XMVector3Length(d)));
                    }
                    for(size_t i=0;i+2<mesh.indices.size();i+=3)
                    {
                        const auto a=DirectX::XMLoadFloat3(&mesh.vertex_positions[mesh.indices[i]]);
                        const auto b=DirectX::XMLoadFloat3(&mesh.vertex_positions[mesh.indices[i+1]]);
                        const auto c=DirectX::XMLoadFloat3(&mesh.vertex_positions[mesh.indices[i+2]]);
                        if(DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(
                            DirectX::XMVector3Cross(b-a,c-a)))<1e-10f)++degenerate;
                    }
                    return std::make_pair(error,degenerate);
                };
                WatercourseSceneCommand raw(scene,std::make_unique<SetTransformCommand>(
                    scene,nodes[selected],before,after));
                Require(raw.Execute(),"raw node move reproduction refused");
                poseCamera({8,55,-33},{0,29,0});capture("node-raw-jagged.png");
                const auto rawQuality=quality();
                raw.Undo();
                RiverNodeMovePreview preview(scene,nodes[selected],before);
                auto larger=after;larger.translation.z+=5;
                Require(preview.Apply(larger),"large live node preview failed");
                for(int i=0;i<8;++i)app.Run();
                Require(preview.Apply(after),"smaller live node preview failed");
                for(int i=0;i<8;++i)app.Run();
                gpu->WaitForGPU();
                const auto previewCurve=SampleSplineWorldPoints(scene,river,512);
                auto p0=terrainBefore,p1=CaptureTerrainSculpt(scene,*terrain);
                Require(RetainChangedTerrainSculpt(p0,p1)==0,"live preview changed terrain before commit");
                preview.Restore();
                for(size_t i=0;i<nodes.size();++i)
                {
                    const auto p=CaptureTransform(*scene.transforms.GetComponent(nodes[i])).translation;
                    Require(std::memcmp(&p,&originalNodes[i].translation,sizeof(p))==0,
                        "cancelled node preview left displaced neighbours");
                }
                Require(history.Execute(std::make_unique<SetWatercourseTransformCommand>(
                    scene,nodes[selected],before,after)),"smooth node move refused");
                poseCamera({8,55,-33},{0,29,0});capture("node-smoothed.png");
                const auto smoothQuality=quality();
                std::cout<<"NODE_EDIT_NATIVE_RIBBON rawCentreError="<<rawQuality.first
                    <<" smoothCentreError="<<smoothQuality.first<<" rawDegenerate="<<rawQuality.second
                    <<" smoothDegenerate="<<smoothQuality.second<<"\n";
                Require(smoothQuality.first<.02f,"smooth native ribbon still departs from sampled curve");
                Require(smoothQuality.second==0,"smooth native ribbon still has collapsed triangles");
                size_t influenced=0;
                for(size_t i=0;i<nodes.size();++i)
                {
                    const auto p=CaptureTransform(*scene.transforms.GetComponent(nodes[i])).translation;
                    if(std::abs(p.z-originalNodes[i].translation.z)>.001f)++influenced;
                }
                Require(influenced>4,"smooth move did not distribute dense control points");
                Require(GrassAndPaintDigest(*terrain)==masks,"smooth node move changed grass or paint");
                const auto smoothTerrain=CaptureTerrainSculpt(scene,*terrain);
                const auto smoothCurve=SampleSplineWorldPoints(scene,river,512);
                Require(previewCurve.size()==smoothCurve.size() &&
                    std::memcmp(previewCurve.data(),smoothCurve.data(),smoothCurve.size()*sizeof(XMFLOAT3))==0,
                    "live preview and committed curve differ");
                Require(history.Undo(),"smooth move Undo failed");
                auto u0=terrainBefore,u1=CaptureTerrainSculpt(scene,*terrain);
                Require(RetainChangedTerrainSculpt(u0,u1)==0,"smooth Undo lost terrain");
                for(size_t i=0;i<nodes.size();++i)
                {
                    const auto p=CaptureTransform(*scene.transforms.GetComponent(nodes[i])).translation;
                    Require(std::memcmp(&p,&originalNodes[i].translation,sizeof(p))==0,"smooth Undo lost node");
                }
                Require(history.Redo(),"smooth move Redo failed");
                auto r0=smoothTerrain,r1=CaptureTerrainSculpt(scene,*terrain);
                Require(RetainChangedTerrainSculpt(r0,r1)==0,"smooth Redo lost terrain");
                const auto redoCurve=SampleSplineWorldPoints(scene,river,512);
                Require(std::memcmp(smoothCurve.data(),redoCurve.data(),smoothCurve.size()*sizeof(XMFLOAT3))==0,
                    "smooth Redo changed curve");
                for(const auto nearEnd:std::vector<size_t>{1,nodes.size()-2})
                {
                    const auto edgeBefore=CaptureTransform(*scene.transforms.GetComponent(nodes[nearEnd]));
                    auto edgeAfter=edgeBefore;edgeAfter.translation.z+=6;
                    RiverNodeMovePreview edgePreview(scene,nodes[nearEnd],edgeBefore);
                    Require(edgePreview.Apply(edgeAfter),"near-end preview failed");
                    for(int i=0;i<8;++i)app.Run();
                    gpu->WaitForGPU();
                    const auto edgeQuality=quality();
                    Require(edgeQuality.first<.02f && edgeQuality.second==0,
                        "near-end smooth move folded the native ribbon");
                    edgePreview.Restore();
                }
                const auto current=CaptureTransform(*scene.transforms.GetComponent(nodes[selected]));
                auto illegal=current;illegal.translation.y-=1000;
                SetWatercourseTransformCommand refused(scene,nodes[selected],current,illegal);
                Require(!refused.Execute(),"unsafe smooth move should refuse below-floor bed");
                const auto rollbackCurve=SampleSplineWorldPoints(scene,river,512);
                Require(std::memcmp(smoothCurve.data(),rollbackCurve.data(),smoothCurve.size()*sizeof(XMFLOAT3))==0,
                    "refused smooth move left neighbouring nodes changed");
                auto bad0=smoothTerrain,bad1=CaptureTerrainSculpt(scene,*terrain);
                Require(RetainChangedTerrainSculpt(bad0,bad1)==0,"refused smooth move changed terrain");
                TransformState spawn;spawn.translation={-18,pick(-18,-5)+1.1f,-5};
                DirectX::XMStoreFloat4(&spawn.rotation,DirectX::XMQuaternionRotationRollPitchYaw(0,DirectX::XM_PIDIV2,0));
                CreatePlayerStartCommand player(scene,spawn);Require(player.Execute(),"node test Player Start failed");
                const auto saved=(out/"node-edited.wiscene").generic_u8string();
                wi::Archive disk(saved,false,false);scene.Serialize(disk);
                Require(disk.SaveFile(saved),"node-edit save failed");
                wi::scene::Scene reopened;
                wi::Archive opened(saved,true,false);reopened.Serialize(opened);
                const auto restoredRiver=reopened.Entity_FindByName("River Water");
                const auto reloadedCurve=SampleSplineWorldPoints(reopened,restoredRiver,512);
                Require(reloadedCurve.size()==smoothCurve.size(),"saved edited curve missing");
                for(size_t i=0;i<smoothCurve.size();++i)
                    Require(DirectX::XMVectorGetX(DirectX::XMVector3Length(
                        DirectX::XMLoadFloat3(&smoothCurve[i])-DirectX::XMLoadFloat3(&reloadedCurve[i])))<.0001f,
                        "save/reopen changed smooth curve");
                std::cout<<"SMOOTH_NODE_PREVIEW_CANCEL_LINKED_BED_UNDO_REDO_REOPEN=PASS influenced="
                    <<influenced<<" total="<<nodes.size()<<"\n";
                return 0;
            }
            char rockyProof[8]{};
            if(GetEnvironmentVariableA("RENEGADE_ROCKY_RIVERBED_GPU_PROOF",
                rockyProof,sizeof(rockyProof))==1 && rockyProof[0]=='1')
            {
                const auto oldPaintCount=terrain->materialEntities.size();
                std::uint64_t paintBefore=14695981039346656037ull;
                for(auto& kv:terrain->chunks)
                    for(const auto& layer:kv.second.blendmap_layers)
                        for(const auto value:layer.pixels)
                            paintBefore=(paintBefore^value)*1099511628211ull;
                const auto heightBefore=CaptureTerrainSculpt(scene,*terrain);
                const auto assetDirectory=fs::absolute(
                    "Content/terrain/riverbed/rocky_rugged").generic_u8string();
                CommandService nativePaintHistory;
                auto paintCommand=std::make_unique<PaintRockyRiverbedCommand>(
                    scene,terrainID,river,assetDirectory);
                auto* paintProbe=paintCommand.get();
                const bool painted=nativePaintHistory.Execute(std::move(paintCommand));
                Require(painted,"native rocky paint command refused disposable linked river");
                for(int frame=0;frame<80;++frame)app.Run();
                gpu->WaitForGPU();
                poseCamera({8,55,-33},{0,29,0});
                capture("mountain-rocky-bed-native.png");
                poseCamera({20,48,25},{0,29,0});
                capture("mountain-rocky-bed-reverse-view.png");
                std::uint64_t originalPaintAfter=14695981039346656037ull;
                for(auto& kv:terrain->chunks)
                    for(size_t layer=0;layer<oldPaintCount;++layer)
                        for(const auto value:kv.second.blendmap_layers[layer].pixels)
                            originalPaintAfter=(originalPaintAfter^value)*
                                1099511628211ull;
                Require(originalPaintAfter==paintBefore,
                    "rocky extra material changed original four paint masks");
                auto heightsNow=CaptureTerrainSculpt(scene,*terrain);
                auto originalHeights=heightBefore;
                Require(RetainChangedTerrainSculpt(originalHeights,heightsNow)==0,
                    "rocky bed mask modified existing sculpt");
                // Only this disposable asset proof gets a PlayerStart:
                // Runtime needs a camera looking down at the rocky banks.
                // NEVER write one to a creator's original WISCENE.
                if(ResolvePlayerStart(scene).resolution ==
                    PlayerStartResolution::Missing)
                {
                    TransformState spawn;
                    spawn.translation={8.f,55.f,-33.f};
                    DirectX::XMStoreFloat4(&spawn.rotation,
                        DirectX::XMQuaternionRotationRollPitchYaw(
                            .65f,-.24f,0.f));
                    CreatePlayerStartCommand playerSpawn(scene,spawn);
                    Require(playerSpawn.Execute(),
                        "disposable rocky TestGame start could not be created");
                }
                const auto save=(out/"rocky-painted-terrain.wiscene").generic_u8string();
                wi::Archive paintArchive;
                scene.Serialize(paintArchive);
                Require(paintArchive.SaveFile(save),
                    "five-material terrain WISCENE save failed");
                {
                    wi::Archive disk(save,true,false);
                    Require(disk.IsOpen(),"native rocky scene reopen failed");
                    wi::scene::Scene opened;
                    opened.Serialize(disk);
                    Require(opened.terrains.GetCount()==1,
                        "rocky scene lost terrain on save");
                    const auto& t=opened.terrains[0];
                    Require(t.materialEntities.size()==5,
                        "rocky fifth material not persisted in WISCENE");
                    size_t masks=0;
                    for(const auto& kv:t.chunks)
                    {
                        Require(kv.second.blendmap_layers.size()==5,
                            "native fifth riverbed mask lost on disk");
                        for(const auto value:kv.second.blendmap_layers[4].pixels)
                            masks+=(value>0);
                    }
                    Require(masks>50,
                        "reopened terrain lost rocky riverbed footprint");
                    std::cout<<"ROCKY_RIVERBED_NATIVE_WISCENE_REOPEN=PASS"
                        <<" fifth_layer_pixels="<<masks<<"\n";
                }
                Require(nativePaintHistory.Undo(),
                    "rocky material Undo rejected");
                Require(terrain->materialEntities.size()==4,
                    "rocky material Undo did not restore original four slots");
                for(const auto& entry:terrain->chunks)
                    Require(entry.second.blendmap_layers.size()==4 &&
                        entry.second.blendmap.IsValid() &&
                        entry.second.blendmap.desc.array_size==4,
                        "rocky material Undo did not restore native four-layer GPU array");
                Require(nativePaintHistory.Redo(),
                    "rocky material Redo rejected");
                Require(terrain->materialEntities.size()==5,
                    "rocky material Redo did not restore fifth terrain slot");
                for(const auto& entry:terrain->chunks)
                    Require(entry.second.blendmap_layers.size()==5 &&
                        entry.second.blendmap.IsValid() &&
                        entry.second.blendmap.desc.array_size==5,
                        "rocky material Redo did not restore native five-layer GPU array");
                std::cout<<"ROCKY_RIVERBED_UNDO_REDO=PASS\n";
                std::cout<<"ROCKY_RIVERBED_OWNER_PAINT_UNALTERED=PASS"
                    <<" TERRAIN_HEIGHT_UNALTERED=PASS\n";
                return 0;
            }
            char clarityProbe[8]{};
            if(GetEnvironmentVariableA("RENEGADE_NATIVE_CLARITY_PROOF",
                clarityProbe,sizeof(clarityProbe))==1 && clarityProbe[0]=='1')
            {
                auto* material=scene.materials.GetComponent(river);
                Require(material!=nullptr,"native-water clarity proof material missing");
                const auto savedColor=material->baseColor;
                const auto savedStrength=material->normalMapStrength;
                const auto colour=DirectX::XMFLOAT4{
                    savedColor.x,savedColor.y,savedColor.z,0.947f};
                material->SetBaseColor(colour);
                material->SetDirty();
                poseCamera({8,55,-33},{0,29,0});
                capture("mountain-native-clarity-0053.png");
                material->SetBaseColor({0.011f,0.044f,0.047f,0.947f});
                material->SetNormalMapStrength(0.804f);
                material->SetDirty();
                poseCamera({8,55,-33},{0,29,0});
                capture("mountain-native-actual-inspector-values.png");
                material->SetBaseColor(savedColor);
                material->SetNormalMapStrength(savedStrength);
                material->SetDirty();
                poseCamera({8,55,-33},{0,29,0});
                capture("mountain-native-clarity-restored.png");
                std::cout<<"NATIVE_CLARITY_ALPHA_LOW_0053=0.947"
                         <<" NATIVE_CLARITY_BASELINE=0.874"
                         <<" DISPOSABLE=PASS\n";
            }
            // Proof-only experiment in the EXISTING Wicked engine ripple pass.
            // This does not install a custom pixel shader or modify scene
            // authoring; the transient GPU stamps are cleared before undo,
            // history, serialization or user/runtime controls are exercised.
            char rippleProbe[8]{};
            if(GetEnvironmentVariableA("RENEGADE_NATIVE_RIPPLE_PROOF",
                rippleProbe,sizeof(rippleProbe))==1 && rippleProbe[0]=='1')
            {
                Require(scene.waterRipples.empty(),"ripple proof has stale stamps");
                for(std::size_t i=6;i+6<planned.points.size();i+=7)
                    scene.PutWaterRipple(planned.points[i]);
                Require(!scene.waterRipples.empty(),"Wicked produced no native ripples");
                std::cout<<"NATIVE_WICKED_RIPPLE_STAMPS="<<scene.waterRipples.size()<<"\n";
                poseCamera({8,55,-33},{0,29,0});
                std::cout << "NATIVE_WICKED_RIPPLE_GPU_RESOURCE=" << render.rtWaterRipple.IsValid()
                    << " LIVE_STAMPS=" << scene.waterRipples.size() << "\n";
                capture("mountain-native-wicked-ripples.png");
                // A near-water perspective verifies the screen-space ripple
                // normals are usable on this sloping fitted river.
                poseCamera({-12,34,-13},{-14,28,0});
                capture("mountain-native-wicked-ripples-close.png");
                scene.waterRipples.clear();
                poseCamera({8,55,-33},{0,29,0});
                capture("mountain-native-wicked-ripples-cleared.png");
                std::cout<<"NATIVE_WICKED_RIPPLE_PASS_DISPOSABLE=PASS\n";
            }
            const auto physicsContact=wi::physics::Intersects(scene,wi::primitive::Ray(
                XMFLOAT3(0,150,0),XMFLOAT3(0,-1,0),0,300));
            Require(physicsContact.IsValid() && std::abs(physicsContact.position.y-pick(0,0))<.02f,
                "mountain Jolt heightfield differs from rendered fitted bed");
            std::cout<<"MOUNTAIN_JOLT_TRIANGLE_CONTACT=PASS\n";
            const auto fittedTerrain=CaptureTerrainSculpt(scene,*terrain);
            const auto* mesh=scene.meshes.GetComponent(river);
            Require(mesh && mesh->vertex_positions.size()>24,"mountain native mesh absent");
            std::ofstream csv(out/"mountain-cross-sections.csv");
            csv<<"sample,x,y,z,leftGround,centreGround,rightGround,centreClearance\n";
            float lastY=INFINITY,minClearance=INFINITY,maxEdgeFloat=-INFINITY;
            const auto vertices=mesh->vertex_positions;
            for(std::size_t i=0;i+1<vertices.size();i+=2)
            {
                const auto a=vertices[i],b=vertices[i+1];
                const XMFLOAT3 c{(a.x+b.x)*.5f,(a.y+b.y)*.5f,(a.z+b.z)*.5f};
                const float left=pick(a.x,a.z),right=pick(b.x,b.z),ground=pick(c.x,c.z);
                Require(std::isfinite(c.y) && c.y<=lastY+.004f,"rendered mountain water turned uphill");
                Require(std::abs(a.y-b.y)<.001f,"cross-section is not level");
                maxEdgeFloat=std::max({maxEdgeFloat,a.y-left,b.y-right});
                if(i>vertices.size()/8 && i<vertices.size()*7/8)
                    minClearance=std::min(minClearance,c.y-ground);
                lastY=c.y;
                csv<<i/2<<','<<c.x<<','<<c.y<<','<<c.z<<','<<left<<','
                    <<ground<<','<<right<<','<<c.y-ground<<'\n';
            }
            csv.close();
            std::cout<<"MOUNTAIN_NATIVE_CENTRE_CLEARANCE="<<minClearance
                <<" MAX_EDGE_FLOAT="<<maxEdgeFloat<<"\n";
            Require(minClearance>.15f,"fitted centreline buried in actual terrain triangles");
            Require(maxEdgeFloat<.08f,"fitted ribbon edge floats above actual channel bank");
            Require(GrassAndPaintDigest(*terrain)==masks,"mountain fit changed grass/paint");
            Require(history.Undo(),"fit Undo failed");
            auto undoA=initial,undoB=CaptureTerrainSculpt(scene,*terrain);
            Require(RetainChangedTerrainSculpt(undoA,undoB)==0,"fit Undo did not restore sculpt byte-for-byte");
            Require(scene.splines.GetComponent(river)->spline_node_entities.size()==points.size(),
                "fit Undo lost original control nodes");
            Require(history.Redo(),"fit Redo failed");
            auto redoA=fittedTerrain,redoB=CaptureTerrainSculpt(scene,*terrain);
            Require(RetainChangedTerrainSculpt(redoA,redoB)==0,"fit Redo changed composed result");
            poseCamera({-27,41,-19},{-8,33,0});capture("mountain-fitted-low.png");
            // Unsupported transforms and boundary crossings fail without edits.
            const auto rootPose=CaptureTransform(*scene.transforms.GetComponent(river));
            scene.transforms.GetComponent(river)->scale_local.x=2;
            scene.transforms.GetComponent(river)->SetDirty();
            Require(!PlanMountainStream(scene,terrainID,river,planned,error),"scaled root accepted");
            scene.transforms.GetComponent(river)->scale_local=rootPose.scale;
            scene.transforms.GetComponent(river)->SetDirty();
            const auto node=scene.splines.GetComponent(river)->spline_node_entities.back();
            const auto oldPose=CaptureTransform(*scene.transforms.GetComponent(node));
            scene.transforms.GetComponent(node)->translation_local.x=1000;
            scene.transforms.GetComponent(node)->SetDirty();
            Require(!PlanMountainStream(scene,terrainID,river,planned,error),"out-of-terrain path accepted");
            scene.transforms.GetComponent(node)->translation_local=oldPose.translation;
            scene.transforms.GetComponent(node)->SetDirty();
            scene.splines.GetComponent(river)->prev_mesh_generation_nodes=-1;
            Require(GrassAndPaintDigest(*terrain)==masks,"refused fit changed masks");
            for(int i=0;i<15;++i)app.Run();gpu->WaitForGPU();
            Require(EnsurePersistentEntityIdentities(scene,error),"mountain persistent IDs failed");
            TransformState playerPose;
            playerPose.translation={-18,pick(-18,-5)+1.1f,-5};
            DirectX::XMStoreFloat4(&playerPose.rotation,
                DirectX::XMQuaternionRotationRollPitchYaw(0,DirectX::XM_PIDIV2,0));
            CreatePlayerStartCommand mountainPlayer(scene,playerPose);
            Require(mountainPlayer.Execute(),"mountain player start failed");
            fs::create_directories(out/"Content"/"Scenes");
            const auto saved=(out/"Content"/"Scenes"/"Mountain.wiscene").generic_u8string();
            wi::Archive disk(saved,false,false);scene.Serialize(disk);
            Require(disk.SaveFile(saved),"mountain disk save failed");
            {
                wi::scene::Scene reopened;
                wi::Archive savedDisk(saved,true,false);reopened.Serialize(savedDisk);
                const auto tid=reopened.terrains.GetEntity(0);
                reopened.terrains[0].scene=&reopened;reopened.terrains[0].terrainEntity=tid;
                Require(RecomposeRiverTerrainLayers(reopened,tid,error),error.c_str());
                const auto rid=reopened.Entity_FindByName("River Water");
                MountainStreamFit again;
                Require(PlanMountainStream(reopened,tid,rid,again,error),error.c_str());
                std::cout<<"MOUNTAIN_WISCENE_REOPEN_BASELINE_AND_REFIT=PASS\n";
            }
            poseCamera({20,48,25},{0,29,0});capture("mountain-fitted-reverse-view.png");
            // A/B isolated diagnostic only: pin the same native scene and
            // camera, toggle ONLY local-water-depth pixel classification.
            // Restore native shader and animation before history/save proof.
            const int localDepthProof=OptionalNativeLocalDepthProofShader();
            if(localDepthProof>=0)
            {
                auto* proofWater=scene.materials.GetComponent(river);
                const float previousRate=proofWater->texAnimFrameRate;
                const float previousGameSpeed=wi::renderer::GetGameSpeed();
                proofWater->texAnimFrameRate=0;
                wi::renderer::SetGameSpeed(0);
                poseCamera({20,48,25},{0,29,0});
                capture("mountain-native-before-local-depth.png");
                proofWater->SetCustomShaderID(localDepthProof);
                proofWater->SetDirty();
                poseCamera({20,48,25},{0,29,0});
                capture("mountain-local-depth-prototype.png");
                proofWater->DisableCustomShader();
                proofWater->SetDirty();
                poseCamera({20,48,25},{0,29,0});
                capture("mountain-native-after-local-depth.png");
                wi::renderer::SetGameSpeed(previousGameSpeed);
                proofWater->texAnimFrameRate=previousRate;
                std::cout<<"PROOF_ONLY_LOCAL_DEPTH_NATIVE_NORMAL_UNCHANGED=PASS\n";
            }
            // Sweep camera height over one downstream reach, including crossing
            // the renderer's shared whole-river reflector height while remaining
            // above the local water surface. Pair normals off/on at each height.
            const auto* heightMesh=scene.meshes.GetComponent(river);
            const auto sample=(heightMesh->vertex_positions.size()/2)*3/4;
            const auto nextSample=std::min(sample+4,heightMesh->vertex_positions.size()/2-1);
            auto centreAt=[&](std::size_t at) {
                const auto& a=heightMesh->vertex_positions[at*2];
                const auto& b=heightMesh->vertex_positions[at*2+1];
                return XMFLOAT3((a.x+b.x)*.5f,(a.y+b.y)*.5f,(a.z+b.z)*.5f);
            };
            const auto localWater=centreAt(sample),lookWater=centreAt(nextSample);
            auto* heightMaterial=scene.materials.GetComponent(river);
            const float savedStrength=heightMaterial->normalMapStrength;
            const auto savedDirection=heightMaterial->texAnimDirection;
            if(localDepthProof>=0)
            {
                const float prevRate=heightMaterial->texAnimFrameRate;
                const float prevSpeed=wi::renderer::GetGameSpeed();
                wi::renderer::SetGameSpeed(0);
                heightMaterial->texAnimFrameRate=0;
                for(int strength=0;strength<2;++strength)
                {
                    heightMaterial->SetNormalMapStrength(strength?1.8f:0.0f);
                    for(int mode=0;mode<2;++mode)
                    {
                        if(mode)heightMaterial->SetCustomShaderID(localDepthProof);
                        else heightMaterial->DisableCustomShader();
                        heightMaterial->SetDirty();
                        poseCamera({localWater.x+1.0f,localWater.y+1.2f,localWater.z-1.5f},lookWater);
                        const auto name=std::string(mode?"local-depth-close-":"native-depth-close-")+
                            (strength?"normal.png":"flat.png");
                        capture(name.c_str());
                    }
                }
                heightMaterial->DisableCustomShader();
                heightMaterial->SetNormalMapStrength(savedStrength);
                heightMaterial->texAnimFrameRate=prevRate;
                wi::renderer::SetGameSpeed(prevSpeed);
                heightMaterial->SetDirty();
                std::cout<<"PROOF_ONLY_LOCAL_WATER_CAMERA_ABOVE=PASS\n";
            }
            heightMaterial->texAnimDirection={0,0};
            const float heights[]={.3f,.6f,1.2f,2.5f,5.f,10.f};
            std::ofstream heightsCsv(out/"camera-height-sweep.csv");
            heightsCsv<<"case,heightAboveLocalWater,cameraY,localWaterY,sharedReflectorY\n";
            for(int h=0;h<6;++h)
            {
                for(int normals=0;normals<2;++normals)
                {
                    heightMaterial->SetNormalMapStrength(normals?savedStrength:0.f);
                    poseCamera({localWater.x,localWater.y+heights[h],localWater.z},lookWater);
                    const auto name=std::string("mountain-height-")+std::to_string(h)
                        +(normals?"-normal.png":"-flat.png");
                    capture(name.c_str());
                }
                heightsCsv<<h<<','<<heights[h]<<','<<localWater.y+heights[h]<<','
                    <<localWater.y<<','<<scene.objects.GetComponent(river)->center.y<<'\n';
            }
            heightMaterial->SetNormalMapStrength(savedStrength);
            heightMaterial->texAnimDirection=savedDirection; heightMaterial->SetDirty();
            std::cout<<"MOUNTAIN_LOCAL_WATER_HEIGHT_SWEEP_CAPTURES=PASS\n";
            Require(GrassAndPaintDigest(*terrain)==masks,"mountain final masks changed");
            std::cout<<"MOUNTAIN_NATIVE_GPU_HISTORY_MASKS_REOPEN=PASS\n";
            return 0;
        }

        // A single known mound makes a deterministic uneven surface without
        // relying on noise or project content. The changed terrain is local.
        bool changed = false;
        for(int stroke=0;stroke<4;++stroke)
            changed |= SculptTerrain(scene,*terrain,{0,0,0},
                22.0f,0.5f,0.45f,TerrainSculptMode::Raise,0);
        Require(changed, "generated terrain could not be sculpted");
        for(int frame=0;frame<20;++frame) app.Run();
        terrain->Generation_Cancel();
        const auto sculpted = CaptureTerrainSculpt(scene,*terrain);
        const auto preservedGrassAndPaint = GrassAndPaintDigest(*terrain);

        std::vector<XMFLOAT3> stroke;
        float lowest=100000,highest=-100000;
        for (int x=-24; x<=24; x+=2)
        {
            // Equivalent to Studio's FILTER_TERRAIN-only ray pick, with a
            // known vertical camera ray through the sculpted landscape.
            const wi::primitive::Ray ray(
                XMFLOAT3(static_cast<float>(x),150.0f,0.0f),
                XMFLOAT3(0,-1,0),0,300);
            const auto hit = wi::scene::Pick(
                ray,wi::enums::FILTER_TERRAIN,~0u,scene);
            Require(hit.entity != wi::ecs::INVALID_ENTITY,
                "terrain-only raycast failed on generated chunk");
            Require(std::isfinite(hit.position.y),"nonfinite terrain height");
            stroke.push_back(hit.position);
            lowest=std::min(lowest,hit.position.y);
            highest=std::max(highest,hit.position.y);
        }
        std::cout << "TERRAIN_RAY_HITS=" << stroke.size()
            << " Y_RANGE=" << lowest << ".." << highest << "\n";
        Require(highest-lowest>0.20f,
            "fixture terrain elevations insufficiently varied");
        Require(IsRiverSplineStrokeValid(stroke),
            "terrain-sampled stroke rejected");

        RiverSplineOptions options;
        options.widthMeters=4.0f;
        auto river=std::make_unique<CreateRiverSplineCommand>(
            scene,stroke,options);
        auto* command=river.get();
        CommandService history;
        Require(history.Execute(std::move(river)),
            "native river command failed on terrain");
        const auto root=command->CreatedEntity();
        for (int frame=0;frame<25;++frame) app.Run();
        gpu->WaitForGPU();
        const auto* spline=scene.splines.GetComponent(root);
        const auto* material=scene.materials.GetComponent(root);
        const auto* mesh=scene.meshes.GetComponent(root);
        Require(spline && material && mesh,
            "terrain river renderable mesh not generated");
        Require(material->shaderType ==
            wi::scene::MaterialComponent::SHADERTYPE_WATER,
            "generated terrain river lost water material");
        Require(material->textures[
            wi::scene::MaterialComponent::NORMALMAP].resource.IsValid(),
            "actual terrain river missing its Wicked water-normal texture");
        Require(material->texAnimFrameRate > 0.0f,
            "actual terrain river has no animated material UVs");
        Require(spline->spline_node_entities.size()==stroke.size(),
            "river control point count mismatch");
        for(std::size_t index=0;index<stroke.size();++index)
        {
            const auto* transform=scene.transforms.GetComponent(
                spline->spline_node_entities[index]);
            Require(transform,"river transform missing");
            Require(std::abs(transform->translation_local.y -
                (stroke[index].y+options.surfaceOffsetMeters))<0.005f,
                "river node elevation differs from terrain raycast");
        }
        std::cout << "RIVER_TERRAIN_HEIGHT_SAMPLES=PASS meshVerts="
            << mesh->vertex_positions.size()<<"\n";
        const auto safePath = (out/"uneven-terrain-river.wiscene").generic_u8string();
        wi::Archive archive;
        scene.Serialize(archive);
        Require(archive.SaveFile(safePath),"scene serialization failed");
        const auto loaded=PrepareWickedSceneOpen(safePath);
        Require(loaded.IsReady(),"scene open validation failed");
        std::cout << "RIVER_TERRAIN_WISCENE=PASS\n";
        auto after=CaptureTerrainSculpt(scene,*terrain);
        auto original=sculpted;
        Require(RetainChangedTerrainSculpt(original,after)==0,
            "river creation unexpectedly changed terrain height data");
        Require(terrain->chunks.size()==chunksBefore,
            "river creation changed terrain chunk count");
        std::cout << "RIVER_TERRAIN_NOT_MUTATED=PASS\n";
        const auto withRiver = out / "river-over-sculpted-terrain.png";
        const auto withoutRiver = out / "sculpted-terrain-without-river.png";
        gpu->WaitForGPU();
        Require(wi::helper::saveTextureToFile(render.GetRenderResult3D(),
            withRiver.generic_u8string()), "GPU capture failed");
        auto* waterObject = scene.objects.GetComponent(root);
        Require(waterObject != nullptr, "river object missing for baseline");
        waterObject->SetRenderable(false);
        for (int frame=0;frame<18;++frame) app.Run();
        gpu->WaitForGPU();
        Require(wi::helper::saveTextureToFile(render.GetRenderResult3D(),
            withoutRiver.generic_u8string()), "baseline capture failed");
        const auto imageBytes = [](const fs::path& file) {
            std::ifstream stream(file, std::ios::binary);
            return std::string(std::istreambuf_iterator<char>(stream),
                std::istreambuf_iterator<char>());
        };
        const auto withBytes = imageBytes(withRiver);
        const auto withoutBytes = imageBytes(withoutRiver);
        Require(!withBytes.empty() && withBytes != withoutBytes,
            "VISUAL FAIL: terrain river and hidden-river images identical");
        std::cout << "RIVER_TERRAIN_GPU_IMAGE_DIFFERENCE=PASS\n";

        // Explicit riverbed carving must never rewrite a heightmap below
        // its configured encoding floor. This long river crosses unraised
        // world-zero terrain and therefore has to fail without mutation.
        const auto originalTerrain = CaptureTerrainSculpt(scene, *terrain);
        gpu->WaitForGPU();
        Require(!CarveTerrainRiverbed(scene, *terrain, root, 0.4f, 2.0f),
            "riverbed carving should reject a path at the heightmap floor");
        auto unchangedLeft = originalTerrain;
        auto unchangedRight = CaptureTerrainSculpt(scene, *terrain);
        Require(RetainChangedTerrainSculpt(unchangedLeft, unchangedRight) == 0,
            "rejected carve mutated existing terrain");
        std::cout << "RIVERBED_FLOOR_GUARD=PASS\n";

        // A short elevated centre section has real vertical room to form a
        // trench. Carve once, record one Undo operation and prove original
        // sculpted heights can be restored byte-for-byte in float space.
        std::vector<XMFLOAT3> elevated;
        for (int i = 10; i <= 14; ++i)
            elevated.push_back(stroke[static_cast<std::size_t>(i)]);
        auto innerRiver = std::make_unique<CreateRiverSplineCommand>(
            scene, elevated, options);
        auto* innerCommand = innerRiver.get();
        Require(history.Execute(std::move(innerRiver)),
            "interior spline creation failed");
        const auto innerID = innerCommand->CreatedEntity();
        for (int frame=0;frame<12;++frame) app.Run();
        gpu->WaitForGPU();
        auto beforeCarve = CaptureTerrainSculpt(scene, *terrain);
        Require(CarveTerrainRiverbed(scene, *terrain, innerID, 0.35f, 2.0f),
            "elevated riverbed carve rejected valid terrain");
        auto afterCarve = CaptureTerrainSculpt(scene, *terrain);
        auto carvedExpected = afterCarve;
        const auto changedChunks =
            RetainChangedTerrainSculpt(beforeCarve, afterCarve);
        Require(changedChunks > 0,
            "carve did not produce a committed terrain height change");
        RefreshTerrainSculptPhysics(scene, *terrain, afterCarve);
        Require(history.RecordExecuted(
            std::make_unique<SculptTerrainCommand>(scene, terrainID,
                std::move(beforeCarve), std::move(afterCarve))),
            "failed to record carve as one terrain command");
        // A close camera is necessary to judge whether a genuine carved
        // depression/bank is visible; the wide creation view hides it.
        camera.TransformCamera(DirectX::XMMatrixInverse(nullptr,
            DirectX::XMMatrixLookAtLH(
                DirectX::XMVectorSet(19, 22, -24, 1),
                DirectX::XMVectorSet(0, 1.0f, 0, 1),
                DirectX::XMVectorSet(0, 1, 0, 0))));
        camera.UpdateCamera();
        for (int frame=0;frame<24;++frame) app.Run();
        gpu->WaitForGPU();
        Require(wi::helper::saveTextureToFile(render.GetRenderResult3D(),
            (out/"carved-actual-terrain.png").generic_u8string()),
            "carved terrain screenshot failed");
        Require(history.Undo(), "carve Undo failed");
        auto restoredA = originalTerrain;
        auto restoredB = CaptureTerrainSculpt(scene, *terrain);
        Require(RetainChangedTerrainSculpt(restoredA, restoredB) == 0,
            "Undo failed to restore original sculpted terrain");
        for (int frame=0;frame<8;++frame) app.Run();
        gpu->WaitForGPU();
        Require(history.Redo(), "carve Redo failed");
        auto redoA = carvedExpected;
        auto redoB = CaptureTerrainSculpt(scene, *terrain);
        Require(RetainChangedTerrainSculpt(redoA, redoB) == 0,
            "Redo failed to restore carved heights");
        Require(terrain->chunks.size() == chunksBefore,
            "carve/Undo/Redo changed terrain chunk count");
        Require(GrassAndPaintDigest(*terrain) == preservedGrassAndPaint,
            "riverbed carving changed authored grass/terrain paint masks");
        Require(scene.splines.GetComponent(innerID)->terrain_modifier_amount == 0,
            "riverbed carver enabled unsafe native terrain regeneration");
        const auto carveSavePath =
            (out/"terrain-with-carved-river.wiscene").generic_u8string();
        wi::Archive carveArchive;
        scene.Serialize(carveArchive);
        Require(carveArchive.SaveFile(carveSavePath),
            "carved WISCENE save failed");
        Require(PrepareWickedSceneOpen(carveSavePath).IsReady(),
            "saved carved river scene failed reopen");
        std::cout << "RIVERBED_CARVE_UNDO_REDO_SAVE=PASS changedChunks="
            << changedChunks << "\n";

        // Exercise a channel crossing a native chunk edge at world X=33.
        // A sculpted hill across both chunks supplies legal heightmap room.
        terrain->Generation_Cancel();
        gpu->WaitForGPU();
        for (int pass = 0; pass < 4; ++pass)
            Require(SculptTerrain(scene, *terrain, {33,0,0}, 20, 0.6f,
                0.45f, TerrainSculptMode::Raise, 0),
                "chunk-seam mound sculpt failed");
        for (int frame = 0; frame < 16; ++frame) app.Run();
        gpu->WaitForGPU();
        terrain->Generation_Cancel();
        std::vector<XMFLOAT3> seamPoints;
        for (int x = 28; x <= 38; x += 2)
        {
            const auto hit = wi::scene::Pick(
                wi::primitive::Ray(
                    XMFLOAT3(static_cast<float>(x),150,0),
                    XMFLOAT3(0,-1,0),0,300),
                wi::enums::FILTER_TERRAIN,~0u,scene);
            Require(hit.entity != wi::ecs::INVALID_ENTITY &&
                hit.position.y > 0.35f,
                "chunk seam test needs raised terrain samples");
            seamPoints.push_back(hit.position);
        }
        auto seamRiver = std::make_unique<CreateRiverSplineCommand>(
            scene, seamPoints, options);
        auto* seamCommand = seamRiver.get();
        Require(history.Execute(std::move(seamRiver)),
            "seam-crossing river creation failed");
        const auto seamID = seamCommand->CreatedEntity();
        for (int frame = 0; frame < 15; ++frame) app.Run();
        gpu->WaitForGPU();
        auto seamBefore = CaptureTerrainSculpt(scene, *terrain);
        Require(CarveTerrainRiverbed(scene, *terrain, seamID, 0.25f, 2.0f),
            "safe carve crossing two terrain chunks failed");
        auto seamAfter = CaptureTerrainSculpt(scene, *terrain);
        auto seamExpected = seamAfter;
        const auto seamChunks = RetainChangedTerrainSculpt(
            seamBefore, seamAfter);
        Require(seamChunks >= 2,
            "riverbed carve did not update both sides of a chunk seam");
        Require(history.RecordExecuted(
            std::make_unique<SculptTerrainCommand>(
                scene, terrainID, std::move(seamBefore),
                std::move(seamAfter))),
            "seam crossing carve history record failed");
        Require(history.Undo(), "seam carve Undo failed");
        Require(history.Redo(), "seam carve Redo failed");
        auto seamA = seamExpected;
        auto seamB = CaptureTerrainSculpt(scene, *terrain);
        Require(RetainChangedTerrainSculpt(seamA,seamB)==0,
            "seam carve Redo changed authored terrain");
        Require(GrassAndPaintDigest(*terrain) == preservedGrassAndPaint,
            "multi-chunk riverbed carve changed grass or painted masks");
        Require(terrain->chunks.size() == chunksBefore,
            "multi-chunk carve deleted terrain chunks");
        std::cout << "RIVERBED_CROSS_CHUNK_UNDO_REDO=PASS changedChunks="
            << seamChunks << "\n";

        // Rendering continues after the carve/Undo/Redo cycle; checked
        // descriptor parity is the guard against the rejected stale-buffer
        // terrain fix. Exercise a disposable Test Game-like serialized
        // snapshot/pause/restore before sculpting again.
        for (int frame = 0; frame < 20; ++frame) app.Run();
        gpu->WaitForGPU();
        auto checkDescriptors = [&]()
        {
            for (const auto& entry : terrain->chunks)
            {
                const auto* mesh = scene.meshes.GetComponent(
                    entry.second.entity);
                if (mesh && !mesh->subsets.empty())
                    Require(scene.geometryArrayMapped &&
                        scene.geometryArrayMapped[mesh->geometryOffset].ib ==
                            mesh->ib.descriptor_srv,
                        "riverbed carve left a stale GPU terrain descriptor");
            }
        };
        checkDescriptors();
        const auto snapshot = (out/"riverbed-return-snapshot.wiscene").generic_u8string();
        wi::Archive snapshotArchive;
        scene.Serialize(snapshotArchive);
        Require(snapshotArchive.SaveFile(snapshot),
            "riverbed snapshot archive failed");
        Require(PrepareWickedSceneOpen(snapshot).IsReady(),
            "riverbed return snapshot failed normal Renegade validation");
        for (int paused=0;paused<20;++paused)
        {
            gpu->SubmitCommandLists();
            wi::renderer::UpdateGPUSuballocator();
        }
        for (int frame=0;frame<20;++frame) app.Run();
        gpu->WaitForGPU();
        terrain->Generation_Cancel();
        const auto beforeReturnSculpt = CaptureTerrainSculpt(scene, *terrain);
        Require(SculptTerrain(scene,*terrain,{33,0,0},5,0.1f,0.4f,
            TerrainSculptMode::Raise,0),
            "could not sculpt after riverbed return simulation");
        for (int frame=0;frame<12;++frame) app.Run();
        gpu->WaitForGPU();
        checkDescriptors();
        Require(ApplyTerrainSculpt(scene,*terrain,beforeReturnSculpt),
            "could not restore terrain after post-return sculpt");
        for (int frame=0;frame<12;++frame) app.Run();
        gpu->WaitForGPU();
        checkDescriptors();
        Require(GrassAndPaintDigest(*terrain) == preservedGrassAndPaint,
            "post-carve return simulation lost grass/paint masks");
        std::cout << "RIVERBED_GPU_DESCRIPTORS_AND_RETURN_SCULPT=PASS\n";

        // The default authored terrain sits at world Y=0, which was the
        // bottom of its R16 envelope. A one-time, undoable encoding-floor
        // extension must preserve ALL mesh vertices and painted masks while
        // allowing a genuine below-zero river along that level ground.
        gpu->WaitForGPU();
        const float initialFloor = terrain->bottomLevel;
        const float rootYBefore =
            scene.transforms.GetComponent(terrainID)->translation_local.y;
        const auto preEnvelopeHeights = CaptureTerrainSculpt(scene,*terrain);
        const auto preEnvelopePaint = GrassAndPaintDigest(*terrain);
        const auto firstChunk = terrain->chunks.begin();
        Require(firstChunk != terrain->chunks.end(), "no envelope test chunks");
        const auto oldEncodedHeights = firstChunk->second.heightmap_data;
        Require(history.Execute(std::make_unique<SetTerrainHeightFloorCommand>(
            scene,terrainID,initialFloor - 10.0f)),
            "explicit 10m terrain height floor extension rejected");
        Require(terrain->bottomLevel == initialFloor - 10.0f,
            "height floor did not extend by 10m");
        Require(!history.Execute(std::make_unique<ExpandTerrainCommand>(
            scene, terrainID)),
            "terrain ring expansion should be guarded after R16 rebasing");
        Require(terrain->chunks.size() == chunksBefore,
            "guarded expansion altered existing terrain chunks");
        Require(scene.transforms.GetComponent(terrainID)->translation_local.y ==
            rootYBefore, "height range extension moved terrain root");
        auto envelopeCompareA = preEnvelopeHeights;
        auto envelopeCompareB = CaptureTerrainSculpt(scene,*terrain);
        Require(RetainChangedTerrainSculpt(
            envelopeCompareA,envelopeCompareB) == 0,
            "height range re-encoding moved authored vertices");
        Require(GrassAndPaintDigest(*terrain) == preEnvelopePaint,
            "height range extension changed vegetation/paint masks");
        Require(firstChunk->second.heightmap_data != oldEncodedHeights,
            "height floor changed without re-encoding native R16 data");
        std::cout << "TERRAIN_HEIGHT_FLOOR_EXTEND=PASS old="
            << initialFloor << " new=" << terrain->bottomLevel << "\n";

        auto beforeZeroCarve = CaptureTerrainSculpt(scene,*terrain);
        Require(CarveTerrainRiverbed(scene,*terrain,root,1.35f,2.0f),
            "default-depth zero-level river could not carve after height floor extension");
        auto afterZeroCarve = CaptureTerrainSculpt(scene,*terrain);
        const auto expectedZeroCarve = afterZeroCarve;
        std::size_t verticesBelowWorldZero = 0;
        float lowestCarvedWorldY = 100000.0f;
        for (const auto& chunk : afterZeroCarve.chunks)
            for (const float y : chunk.heights)
            {
                lowestCarvedWorldY = std::min(lowestCarvedWorldY, y + rootYBefore);
                if (y + rootYBefore < -0.01f)
                    ++verticesBelowWorldZero;
            }
        Require(lowestCarvedWorldY < -0.75f,
            "default 1.35m depth did not create a visible-depth channel");
        Require(verticesBelowWorldZero > 0,
            "riverbed was encoded but no vertices went below world zero");
        const auto zeroChunks = RetainChangedTerrainSculpt(
            beforeZeroCarve,afterZeroCarve);
        Require(zeroChunks > 0,"zero-level river did not lower any terrain");
        Require(history.RecordExecuted(
            std::make_unique<SculptTerrainCommand>(scene, terrainID,
                std::move(beforeZeroCarve),std::move(afterZeroCarve))),
            "failed to record zero-level riverbed carve");
        for(int frame=0;frame<14;++frame) app.Run();
        gpu->WaitForGPU();
        checkDescriptors();
        Require(wi::helper::saveTextureToFile(
            render.GetRenderResult3D(),
            (out/"riverbed-below-world-zero.png").generic_u8string()),
            "below-zero carved river capture failed");
        Require(history.Undo(),"below-zero riverbed Undo failed");
        Require(history.Undo(),"height range extension Undo failed");
        Require(terrain->bottomLevel == initialFloor,
            "Undo did not restore old 16-bit encoding floor");
        const auto* metadataAfterUndo =
            scene.metadatas.GetComponent(terrainID);
        Require(metadataAfterUndo == nullptr ||
            !metadataAfterUndo->string_values.has(
                "renegade.terrain.authored-floor-original"),
            "Undo left the terrain expansion compatibility guard active");
        auto restoredEnvelopeA = preEnvelopeHeights;
        auto restoredEnvelopeB = CaptureTerrainSculpt(scene,*terrain);
        Require(RetainChangedTerrainSculpt(
            restoredEnvelopeA,restoredEnvelopeB) == 0,
            "Undo chain failed to preserve original authored heights");
        Require(firstChunk->second.heightmap_data == oldEncodedHeights,
            "Undo did not restore original R16 heightmap encoding");
        Require(history.Redo(),"height range extension Redo failed");
        Require(history.Redo(),"below-zero riverbed Redo failed");
        auto redoZeroA = expectedZeroCarve;
        auto redoZeroB = CaptureTerrainSculpt(scene,*terrain);
        Require(RetainChangedTerrainSculpt(redoZeroA,redoZeroB) == 0,
            "height floor/carve Redo lost carved vertices");
        Require(GrassAndPaintDigest(*terrain) == preEnvelopePaint,
            "height floor/carve Undo/Redo touched grass or paint");
        Require(terrain->chunks.size() == chunksBefore,
            "height floor/carve changed resident chunk count");
        for(int frame=0;frame<12;++frame) app.Run();
        gpu->WaitForGPU();
        checkDescriptors();
        const auto lowSave = (out/"riverbed-below-zero.wiscene").generic_u8string();
        wi::Archive lowArchive;
        scene.Serialize(lowArchive);
        Require(lowArchive.SaveFile(lowSave),
            "could not save expanded-height river scene");
        Require(PrepareWickedSceneOpen(lowSave).IsReady(),
            "expanded-height river scene failed normal Renegade open");
        {
            wi::Archive reopenedFile(lowSave, true, false);
            Require(reopenedFile.IsOpen(),
                "could not open saved below-zero WISCENE bytes");
            wi::scene::Scene reopened;
            reopened.Serialize(reopenedFile);
            Require(reopened.terrains.GetCount() == 1,
                "reopened below-zero scene lost terrain component");
            const auto& loadedTerrain = reopened.terrains[0];
            Require(std::abs(loadedTerrain.bottomLevel -
                (initialFloor - 10.0f)) < 0.0001f,
                "reopened scene lost extended encoding floor");
            // TerrainComponent::terrainEntity is a runtime-only binding and
            // is populated when the reopened scene becomes active. The
            // serialized root owner is the ECS component-manager entity.
            const auto reopenedTerrainEntity = reopened.terrains.GetEntity(0);
            const auto* reopenedRoot =
                reopened.transforms.GetComponent(reopenedTerrainEntity);
            std::cout << "REOPEN_TERRAIN_ROOT nativeEntity="
                << terrainID << " loadedTerrainEntity="
                << reopenedTerrainEntity
                << " runtimeBinding=" << loadedTerrain.terrainEntity
                << " rootBeforeY=" << rootYBefore
                << " rootAfterY="
                << (reopenedRoot ? reopenedRoot->translation_local.y : -999)
                << " rootAfterWorldY="
                << (reopenedRoot ? DirectX::XMVectorGetY(reopenedRoot->GetWorldMatrix().r[3]) : -999)
                << "\n";
            Require(reopenedRoot &&
                std::abs(reopenedRoot->translation_local.y -
                    rootYBefore) < 0.0001f,
                "reopened below-zero scene moved original terrain root");
            Require(loadedTerrain.chunks.size() == chunksBefore,
                "reopened below-zero scene lost chunk height data");
            const auto* loadedMarker =
                reopened.metadatas.GetComponent(reopenedTerrainEntity);
            Require(loadedMarker &&
                loadedMarker->string_values.has(
                    "renegade.terrain.authored-floor-original"),
                "reopened below-zero scene lost expansion compatibility guard");
        }
        std::cout << "RIVERBED_BELOW_WORLD_ZERO_UNDO_REDO_SAVE=PASS"
            << " changedChunks=" << zeroChunks << "\n";

        // A sparse U bend exposes the old straight-handle-chord carver:
        // inspect actual rendered ribbon centre points against terrain hits.
        const auto curvedBefore = CaptureTerrainSculpt(scene, *terrain);
        auto curved = std::make_unique<CreateRiverSplineCommand>(scene,
            std::vector<XMFLOAT3>{{-20,0,-12}, {-12,0,12},
                {12,0,12}, {20,0,-12}}, options);
        auto* curvedCommand = curved.get();
        Require(history.Execute(std::move(curved)), "curved river creation failed");
        const auto curvedID = curvedCommand->CreatedEntity();
        for (int frame=0; frame<12; ++frame) app.Run();
        gpu->WaitForGPU();
        Require(CarveTerrainRiverbed(scene, *terrain, curvedID, 1.35f, 2.0f),
            "curved river carve failed");
        auto curvedAfter = CaptureTerrainSculpt(scene, *terrain);
        auto curvedUndo = curvedBefore;
        Require(RetainChangedTerrainSculpt(curvedUndo, curvedAfter) > 0,
            "curved carve changed no terrain");
        RefreshTerrainSculptPhysics(scene, *terrain, curvedAfter);
        Require(history.RecordExecuted(std::make_unique<SculptTerrainCommand>(
            scene, terrainID, std::move(curvedUndo), std::move(curvedAfter))),
            "curved carve history failed");
        for (int frame=0; frame<12; ++frame) app.Run();
        gpu->WaitForGPU();
        const auto* curvedMesh = scene.meshes.GetComponent(curvedID);
        Require(curvedMesh && curvedMesh->vertex_positions.size() >= 8,
            "curved water mesh missing");
        std::size_t curvedHits = 0;
        for (std::size_t i=2; i+3<curvedMesh->vertex_positions.size(); i+=2)
        {
            const auto& a = curvedMesh->vertex_positions[i];
            const auto& b = curvedMesh->vertex_positions[i+1];
            const XMFLOAT3 centre((a.x+b.x)*0.5f, (a.y+b.y)*0.5f,
                (a.z+b.z)*0.5f);
            const auto hit = wi::scene::Pick(wi::primitive::Ray(
                XMFLOAT3(centre.x, 30, centre.z), XMFLOAT3(0,-1,0), 0, 60),
                wi::enums::FILTER_TERRAIN, ~0u, scene);
            Require(hit.entity != wi::ecs::INVALID_ENTITY &&
                hit.position.y < centre.y - 1.0f,
                "terrain blocks rendered curved river centreline");
            ++curvedHits;
        }
        Require(curvedHits >= 6, "insufficient curved ribbon terrain checks");
        Require(history.Undo(), "curved bed Undo failed");
        auto curvedRestoredA = curvedBefore;
        auto curvedRestoredB = CaptureTerrainSculpt(scene, *terrain);
        Require(RetainChangedTerrainSculpt(curvedRestoredA, curvedRestoredB) == 0,
            "curved bed Undo lost authored heights");
        std::cout << "RIVERBED_RENDERED_CURVE_CLEARANCE=PASS hits="
            << curvedHits << "\n";

        // Persistent, composited layers on an untouched region of real terrain.
        // Start with the existing sculpted landscape as base, never procedural regeneration.
        gpu->WaitForGPU();
        terrain->SetPhysicsEnabled(true);
        terrain->physics_generation=2;
        wi::physics::SetEnabled(true);
        wi::physics::SetSimulationEnabled(false);
        const auto layersOriginal = CaptureTerrainSculpt(scene,*terrain);
        const auto layersMasks = GrassAndPaintDigest(*terrain);
        CommandService layerHistory;
        auto linked = std::make_unique<CreateRiverSplineCommand>(scene,
            std::vector<XMFLOAT3>{{-20,0,50},{0,0,50},{20,0,50}}, options);
        auto* linkedCreate = linked.get();
        Require(layerHistory.Execute(std::move(linked)), "layer river creation failed");
        const auto linkedID = linkedCreate->CreatedEntity();
        std::string layerError;
        Require(AssignNewPersistentEntityId(scene,linkedID,layerError),
            "could not assign persistent layer river identity");
        for(int i=0;i<8;++i) app.Run();
        gpu->WaitForGPU();
        auto layerLink=std::make_unique<SetRiverTerrainLayerCommand>(scene,terrainID,linkedID,1.35f,2.0f);
        const bool linkPassed=layerHistory.Execute(std::move(layerLink));
        Require(linkPassed,"linking persistent river terrain layer failed");
        camera.TransformCamera(DirectX::XMMatrixInverse(nullptr,DirectX::XMMatrixLookAtLH(
            DirectX::XMVectorSet(25,14,30,1),DirectX::XMVectorSet(0,0,60,1),
            DirectX::XMVectorSet(0,1,0,0))));
        camera.UpdateCamera();
        for(int i=0;i<12;++i) app.Run();
        gpu->WaitForGPU();
        Require(wi::helper::saveTextureToFile(render.GetRenderResult3D(),
            (out/"linked-river-before-move.png").generic_u8string()),"linked river GPU capture failed");
        const auto linkedState=CaptureTerrainSculpt(scene,*terrain);
        Require(!linkedState.riverLayerDocument.empty(),"linked terrain lost baseline payload");
        Require(RecomposeRiverTerrainLayers(scene,terrainID,layerError),"idempotent layer recomposition failed");
        auto idempotentA=linkedState,idempotentB=CaptureTerrainSculpt(scene,*terrain);
        Require(RetainChangedTerrainSculpt(idempotentA,idempotentB)==0,
            "recomposing unchanged river deepened terrain or rewrote baseline");
        Require(layerHistory.Undo(),"river layer Undo failed");
        auto unlinkedA=layersOriginal,unlinkedB=CaptureTerrainSculpt(scene,*terrain);
        Require(RetainChangedTerrainSculpt(unlinkedA,unlinkedB)==0,
            "river layer Undo did not restore original sculpt exactly");
        Require(layerHistory.Redo(),"river layer Redo failed");
        const auto beforeHiddenSculpt=CaptureTerrainSculpt(scene,*terrain);
        Require(SculptTerrain(scene,*terrain,{0,0,50},1.0f,.6f,.4f,TerrainSculptMode::Raise,0),
            "manual baseline sculpt beneath linked channel failed");
        const auto afterHiddenSculpt=CaptureTerrainSculpt(scene,*terrain);
        auto hiddenOutputA=beforeHiddenSculpt,hiddenOutputB=afterHiddenSculpt;
        hiddenOutputB.riverLayerDocument=hiddenOutputA.riverLayerDocument;
        Require(RetainChangedTerrainSculpt(hiddenOutputA,hiddenOutputB)==0,
            "manual base sculpt unexpectedly moved fully masked channel vertices");
        Require(beforeHiddenSculpt.riverLayerDocument!=afterHiddenSculpt.riverLayerDocument,
            "manual sculpt beneath channel did not update baseline");
        auto hiddenBefore=beforeHiddenSculpt,hiddenAfter=afterHiddenSculpt;
        Require(RetainChangedTerrainSculpt(hiddenBefore,hiddenAfter)>0,
            "baseline-only edit could not enter sculpt history");
        Require(layerHistory.RecordExecuted(std::make_unique<SculptTerrainCommand>(
            scene,terrainID,std::move(hiddenBefore),std::move(hiddenAfter))),
            "baseline sculpt history registration failed");
        Require(layerHistory.Undo(),"baseline sculpt Undo failed");
        Require(CaptureTerrainSculpt(scene,*terrain).riverLayerDocument==beforeHiddenSculpt.riverLayerDocument,
            "baseline sculpt Undo lost layer state");
        Require(layerHistory.Redo(),"baseline sculpt Redo failed");
        const auto linkedBeforeMove=CaptureTerrainSculpt(scene,*terrain);
        const auto poseBeforeMove=CaptureTransform(*scene.transforms.GetComponent(linkedID));
        auto poseAfterMove=poseBeforeMove; poseAfterMove.translation.z+=18;
        Require(layerHistory.Execute(std::make_unique<SetWatercourseTransformCommand>(
            scene,linkedID,poseBeforeMove,poseAfterMove)),
            "moving river and terrain as one transaction failed");
        const auto linkedAfterMove=CaptureTerrainSculpt(scene,*terrain);
        Require(layerHistory.Undo(),"linked river move Undo failed");
        auto moveUndoA=linkedBeforeMove,moveUndoB=CaptureTerrainSculpt(scene,*terrain);
        Require(RetainChangedTerrainSculpt(moveUndoA,moveUndoB)==0 &&
            scene.transforms.GetComponent(linkedID)->translation_local.z==poseBeforeMove.translation.z,
            "linked move Undo did not restore both geometry and terrain baseline");
        Require(layerHistory.Redo(),"linked river move Redo failed");
        auto moveRedoA=linkedAfterMove,moveRedoB=CaptureTerrainSculpt(scene,*terrain);
        Require(RetainChangedTerrainSculpt(moveRedoA,moveRedoB)==0,
            "linked move Redo did not restore composed terrain exactly");
        for(int i=0;i<10;++i) app.Run();
        gpu->WaitForGPU();
        checkDescriptors();
        const auto moundHit=wi::scene::Pick(wi::primitive::Ray(
            XMFLOAT3(0,30,50),XMFLOAT3(0,-1,0),0,60),wi::enums::FILTER_TERRAIN,~0u,scene);
        Require(moundHit.entity!=wi::ecs::INVALID_ENTITY && moundHit.position.y>.5f,
            "moving channel failed to reveal later manual baseline sculpt");
        const auto cutHit=wi::scene::Pick(wi::primitive::Ray(
            XMFLOAT3(0,30,68),XMFLOAT3(0,-1,0),0,60),wi::enums::FILTER_TERRAIN,~0u,scene);
        Require(cutHit.entity!=wi::ecs::INVALID_ENTITY && cutHit.position.y < -1.0f,
            "moved channel did not cut its new footprint");
        const auto physicsMound=wi::physics::Intersects(scene,wi::primitive::Ray(
            XMFLOAT3(0,30,50),XMFLOAT3(0,-1,0),0,60));
        const auto physicsCut=wi::physics::Intersects(scene,wi::primitive::Ray(
            XMFLOAT3(0,30,68),XMFLOAT3(0,-1,0),0,60));
        Require(physicsMound.IsValid() && physicsCut.IsValid() &&
            std::abs(physicsMound.position.y-moundHit.position.y)<.02f &&
            std::abs(physicsCut.position.y-cutHit.position.y)<.02f,
            "actual Jolt heightfields did not follow moved channel/restored mound");
        std::cout << "PERSISTENT_RIVER_LAYER_JOLT_CONTACT_PARITY=PASS\n";
        Require(GrassAndPaintDigest(*terrain)==layersMasks,"river layers changed grass/material masks");
        Require(wi::helper::saveTextureToFile(render.GetRenderResult3D(),
            (out/"linked-river-after-move-base-mound-restored.png").generic_u8string()),
            "moved river GPU capture failed");
        // The same wrapper also owns width and node-count changes.
        auto wider=CaptureSpline(*scene.splines.GetComponent(linkedID));wider.width+=1;
        Require(layerHistory.Execute(std::make_unique<WatercourseSceneCommand>(scene,
            std::make_unique<SetSplineStateCommand>(scene,linkedID,wider))),
            "linked river width edit failed");
        Require(layerHistory.Undo(),"linked width Undo failed");
        auto widthA=linkedAfterMove,widthB=CaptureTerrainSculpt(scene,*terrain);
        Require(RetainChangedTerrainSculpt(widthA,widthB)==0,
            "width Undo did not restore old banks and baseline");
        Require(layerHistory.Execute(std::make_unique<WatercourseSceneCommand>(scene,
            std::make_unique<AddSplineNodeCommand>(scene,linkedID))),
            "linked river node insertion failed");
        Require(layerHistory.Undo(),"linked node insertion Undo failed");
        auto addA=linkedAfterMove,addB=CaptureTerrainSculpt(scene,*terrain);
        Require(RetainChangedTerrainSculpt(addA,addB)==0,"node insertion Undo lost terrain");
        Require(layerHistory.Execute(std::make_unique<WatercourseSceneCommand>(scene,
            std::make_unique<RemoveLastSplineNodeCommand>(scene,linkedID))),
            "linked river last-node removal failed");
        Require(layerHistory.Undo(),"linked node removal Undo failed");
        auto removeA=linkedAfterMove,removeB=CaptureTerrainSculpt(scene,*terrain);
        Require(RetainChangedTerrainSculpt(removeA,removeB)==0,"node removal Undo lost terrain");
        std::cout << "PERSISTENT_RIVER_LAYER_WIDTH_NODE_HISTORY=PASS\n";
        TransformState playerPose;playerPose.translation={-6,3,56};
        CreatePlayerStartCommand proofPlayer(scene,playerPose);
        Require(proofPlayer.Execute(),"disposable GUI/Runtime player start creation failed");
        Require(EnsurePersistentEntityIdentities(scene,layerError),
            "disposable scene persistent identities failed");
        const auto layerSave=(out/"persistent-river-layers.wiscene").generic_u8string();
        wi::Archive layerArchive(layerSave,false,false);
        scene.Serialize(layerArchive);
        Require(layerArchive.SaveFile(layerSave),"persistent layer scene save failed");
        {
            wi::scene::Scene reopened;
            wi::Archive disk(layerSave,true,false); reopened.Serialize(disk);
            const auto loadedID=reopened.terrains.GetEntity(0);
            auto& loadedTerrain=reopened.terrains[0];
            // Native terrain update binds these runtime-only pointers on first frame.
            // Bind them explicitly for this read-only fresh-scene validation.
            loadedTerrain.scene=&reopened; loadedTerrain.terrainEntity=loadedID;
            TerrainSculptState loadedBase;
            Require(ReadRiverTerrainBaseline(reopened,loadedID,loadedBase,layerError),
                "fresh scene could not resolve coordinate-based persistent baseline");
            Require(loadedBase.chunks.size()==terrain->chunks.size(),"reopened layer baseline lost chunks");
            Require(RecomposeRiverTerrainLayers(reopened,loadedID,layerError),
                "reopened stable-ID river layer failed to resolve/recompose");
        }
        // Deletion must remove the cut, and Undo must restore the same river
        // UUID and composed heights rather than leaving a ghost trench.
        Require(layerHistory.Execute(std::make_unique<WatercourseSceneCommand>(scene,
            std::make_unique<DeleteEntityCommand>(scene,linkedID))),
            "linked river delete transaction failed");
        Require(!scene.splines.Contains(linkedID),"linked delete left spline alive");
        const auto deletedLayerState=CaptureTerrainSculpt(scene,*terrain);
        TerrainSculptState deletedBaseline;
        Require(ReadRiverTerrainBaseline(scene,terrainID,deletedBaseline,layerError),
            "deleted river baseline could not be read");
        auto deleteHeightA=deletedLayerState,deleteHeightB=deletedBaseline;
        deleteHeightB.riverLayerDocument=deleteHeightA.riverLayerDocument;
        Require(RetainChangedTerrainSculpt(deleteHeightA,deleteHeightB)==0,
            "deleted river left a ghost trench");
        Require(layerHistory.Undo() && scene.splines.Contains(linkedID),
            "linked river delete Undo failed");
        auto deleteUndoA=linkedAfterMove,deleteUndoB=CaptureTerrainSculpt(scene,*terrain);
        Require(RetainChangedTerrainSculpt(deleteUndoA,deleteUndoB)==0,
            "delete Undo did not restore exact linked terrain state");
        Require(layerHistory.Redo() && !scene.splines.Contains(linkedID),
            "linked river delete Redo failed");
        Require(layerHistory.Undo(),"linked river restore for overlap failed");
        std::cout << "PERSISTENT_RIVER_LAYER_DELETE_UNDO_REDO=PASS\n";

        // Overlapping layers compose independently against the same baseline.
        auto crossing=std::make_unique<CreateRiverSplineCommand>(scene,
            std::vector<XMFLOAT3>{{-15,0,55},{0,0,68},{15,0,80}},options);
        auto* crossingCreate=crossing.get();
        Require(layerHistory.Execute(std::move(crossing)),"overlap river creation failed");
        const auto crossingID=crossingCreate->CreatedEntity();
        Require(AssignNewPersistentEntityId(scene,crossingID,layerError),"overlap stable identity failed");
        for(int i=0;i<8;++i) app.Run();
        gpu->WaitForGPU();
        Require(SetRiverTerrainLayer(scene,terrainID,crossingID,2.0f,2.0f,true,layerError),
            "overlapping river layer rejected");
        const auto overlapExpected=CaptureTerrainSculpt(scene,*terrain);
        Require(RecomposeRiverTerrainLayers(scene,terrainID,layerError),"overlap repeat composition failed");
        auto overlapA=overlapExpected,overlapB=CaptureTerrainSculpt(scene,*terrain);
        Require(RetainChangedTerrainSculpt(overlapA,overlapB)==0,
            "overlap repeated composition progressively deepened terrain");
        Require(SetRiverTerrainLayer(scene,terrainID,linkedID,1.35f,2.0f,false,layerError),
            "removing first overlapping channel failed");
        Require(SetRiverTerrainLayer(scene,terrainID,linkedID,1.35f,2.0f,true,layerError),
            "readding first overlapping channel failed");
        auto orderA=overlapExpected,orderB=CaptureTerrainSculpt(scene,*terrain);
        // Layer ordering in metadata may differ, but every rendered height must match.
        orderB.riverLayerDocument=orderA.riverLayerDocument;
        Require(RetainChangedTerrainSculpt(orderA,orderB)==0,
            "overlapping river output depends on insertion order");
        Require(SetRiverTerrainLayer(scene,terrainID,crossingID,2.0f,2.0f,false,layerError),
            "removing overlapping layer failed");
        auto singleA=linkedAfterMove,singleB=CaptureTerrainSculpt(scene,*terrain);
        Require(RetainChangedTerrainSculpt(singleA,singleB)==0,
            "removing overlap failed to restore surviving river and baseline exactly");
        std::cout << "PERSISTENT_RIVER_LAYER_OVERLAP_ORDER_REMOVE=PASS\n";

        const auto beforeBadMove=CaptureTerrainSculpt(scene,*terrain);
        const auto goodPose=CaptureTransform(*scene.transforms.GetComponent(linkedID));
        auto badPose=goodPose;badPose.translation.y-=30;
        Require(!layerHistory.Execute(std::make_unique<SetWatercourseTransformCommand>(
            scene,linkedID,goodPose,badPose)),"below-floor linked move should be rejected");
        auto failedMoveA=beforeBadMove,failedMoveB=CaptureTerrainSculpt(scene,*terrain);
        Require(RetainChangedTerrainSculpt(failedMoveA,failedMoveB)==0 &&
            scene.transforms.GetComponent(linkedID)->translation_local.y==goodPose.translation.y,
            "failed linked move did not roll back transform and terrain atomically");
        const auto layerPayload=CaptureTerrainSculpt(scene,*terrain).riverLayerDocument;
        scene.metadatas.GetComponent(terrainID)->string_values.set(RiverTerrainLayersMetadataKey,"{broken");
        Require(!RecomposeRiverTerrainLayers(scene,terrainID,layerError),
            "malformed layer payload was not rejected");
        scene.metadatas.GetComponent(terrainID)->string_values.set(RiverTerrainLayersMetadataKey,layerPayload);
        const auto duplicate=scene.Entity_CreateTransform("Duplicate UUID refusal fixture");
        scene.metadatas.Create(duplicate).string_values.set(PersistentEntityIdMetadataKey,
            PersistentEntityId(scene,linkedID));
        Require(!RecomposeRiverTerrainLayers(scene,terrainID,layerError),
            "duplicate persistent river UUID was not rejected");
        scene.Entity_Remove(duplicate,true);
        auto refusalA=beforeBadMove,refusalB=CaptureTerrainSculpt(scene,*terrain);
        Require(RetainChangedTerrainSculpt(refusalA,refusalB)==0,
            "malformed/ambiguous layer refusal changed terrain");
        std::cout << "PERSISTENT_RIVER_LAYER_FAILURE_ROLLBACK_SCHEMA_UUID=PASS\n";

        // Unknown terrain mutation must never be silently overwritten by a refresh.
        auto& conflictChunk=terrain->chunks.begin()->second;
        auto* conflictMesh=scene.meshes.GetComponent(conflictChunk.entity);
        const float conflictBefore=conflictMesh->vertex_positions[0].y;
        conflictMesh->vertex_positions[0].y+=.25f;
        Require(!RecomposeRiverTerrainLayers(scene,terrainID,layerError),
            "layer compositor overwrote an unknown external terrain edit");
        Require(conflictMesh->vertex_positions[0].y==conflictBefore+.25f,
            "refused layer operation changed the conflicting vertex");
        conflictMesh->vertex_positions[0].y=conflictBefore;
        Require(SetRiverTerrainLayer(scene,terrainID,linkedID,1.35f,2.0f,false,layerError),
            "removing river layer failed to restore base landscape");
        Require(!HasRiverTerrainLayers(scene,terrainID),"last layer removal retained a live baseline");
        Require(GrassAndPaintDigest(*terrain)==layersMasks,"layer removal changed grass/material masks");
        std::cout << "PERSISTENT_RIVER_LAYER_BASE_MOVE_SCULPT_UNDO_REOPEN_CONFLICT=PASS\n";

        std::cout << "PASS: sampled real uneven terrain, authored water ribbon, "
            << "preserved terrain and saved scene\n";
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr<<"FAIL: "<<e.what()<<"\n";
        return 1;
    }
}
