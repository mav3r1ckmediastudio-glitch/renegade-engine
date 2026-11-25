#pragma once
#include "renegade/bridge/VegetationService.h"
#include "renegade/bridge/SceneService.h"
#include "renegade/bridge/PlayerService.h"
#include "renegade/bridge/PlayerViewRig.h"
#include "renegade/bridge/TerrainPropCollisionWorld.h"
#include "renegade/bridge/ProjectService.h"
#include <filesystem>
#include <map>
#include <chrono>
#include <thread>
#include <iostream>
#include "StockPlantProfilesProof.h"

namespace grass_palette_proof
{
    using namespace renegade::bridge;
    namespace fs = std::filesystem;
    using Mask = std::vector<float>;
    using Masks = std::map<std::string,Mask>;

    inline void Check(bool condition, const std::string& error)
    {
        if (!condition) throw std::runtime_error(error);
    }

    inline Masks Capture(const wi::scene::Scene& scene)
    {
        Masks masks;
        for (size_t i=0; i<scene.hairs.GetCount(); ++i)
        {
            const auto id=scene.hairs.GetEntity(i);
            const auto* name=scene.names.GetComponent(id);
            if (!name || name->name!="Painted grass") continue;
            const auto& hair=scene.hairs[i];
            const auto& slot=scene.materials.GetComponent(id)->textures[wi::scene::MaterialComponent::BASECOLORMAP];
            Check(slot.resource.IsValid() && slot.resource.GetTexture().IsValid(),"Palette texture missing");
            Check(hair.atlas_rects.size()==2 && hair.strandCount>0 && scene.meshes.Contains(hair.meshID),
                "Invalid native hair instance");
            const auto key=fs::u8path(slot.name).filename().u8string();
            Check(masks.emplace(key,hair.vertex_lengths).second,"Unexpected second chunk in one-chunk proof");
        }
        return masks;
    }

    // Read the actual atlas UV and root position produced by the compute shader.
    // Counts alone would miss a patch with all the white plants on one side.
    inline std::vector<unsigned> VerifyMix(const wi::HairParticleSystem& hair)
    {
        using namespace wi::graphics;
        using UV = wi::scene::MeshComponent::Vertex_UVS;
        wi::vector<uint8_t> bytes;
        Check(wi::helper::saveBufferToMemory(hair.generalBuffer, bytes), "Mixed strand readback failed");
        const size_t stride = (2 + 2 * hair.segmentCount) * hair.billboardCount;
        const unsigned n = unsigned(hair.atlas_rects.size());
        const unsigned side = unsigned(std::ceil(std::sqrt(double(n))));
        const size_t posStride = GetFormatStride(hair.position_format);
        const auto& pos = hair.vb_pos[0];
        const auto& uv = hair.vb_uvs;
        Check(n > 1 && hair.strandCount >= n && pos.offset + pos.size <= bytes.size() &&
            uv.offset + uv.size <= bytes.size() && size_t(hair.strandCount) * stride * sizeof(UV) <= uv.size &&
            size_t(hair.strandCount) * stride * posStride <= pos.size, "Incomplete mixed-strand buffers");
        std::vector<unsigned> frames, totals(n);
        std::vector<XMFLOAT2> roots;
        XMFLOAT2 centre{};
        for (unsigned i = 0; i < hair.strandCount; ++i)
        {
            const size_t v = i * stride;
            UV sprite{};
            std::memcpy(&sprite, bytes.data() + uv.offset + v * sizeof(UV), sizeof(UV));
            // The first root vertex is at the lower-left corner of its sprite.
            const int column = int(std::lround(sprite.uv0.x * side / 65535.f));
            const int row = int(std::lround(sprite.uv0.y * side / 65535.f)) - 1;
            Check(column >= 0 && column < int(side) && row >= 0 && row * side + column < n,
                "Mixed grass sampled an empty atlas cell");
            frames.push_back(row * side + column);
            ++totals[frames.back()];
            XMFLOAT2 root{};
            const auto* xyz = bytes.data() + pos.offset + v * posStride;
            if (IsFormatUnorm(hair.position_format))
            {
                uint16_t coords[3]; std::memcpy(coords, xyz, sizeof(coords));
                root = {float(coords[0]), float(coords[2])};
            }
            else
            {
                float coords[3]; std::memcpy(coords, xyz, sizeof(coords));
                root = {coords[0], coords[2]};
            }
            roots.push_back(root); centre.x += root.x; centre.y += root.y;
        }
        auto spread = std::minmax_element(totals.begin(), totals.end());
        Check(*spread.second - *spread.first <= 1, "Painted plant counts differ by more than one");
        centre.x /= hair.strandCount; centre.y /= hair.strandCount;
        std::vector<std::vector<unsigned>> quadrants(4, std::vector<unsigned>(n));
        for (size_t i = 0; i < roots.size(); ++i)
            ++quadrants[(roots[i].x > centre.x) + 2 * (roots[i].y > centre.y)][frames[i]];
        if (n == 2)
        {
            for (const auto& q : quadrants)
            {
                const unsigned count = q[0] + q[1];
                Check(count >= 12 && std::min(q[0], q[1]) >= count / 5,
                    "Mixed patch contains a single-variety region");
            }
        }
        std::cout << "GRASS_GPU_MIX frames=" << n << " min=" << *spread.first
            << " max=" << *spread.second << " strands=" << frames.size() << std::endl;
        return frames;
    }

    inline void VerifyPlayerWalk(SceneService& loaded)
    {
        namespace native = wi::physics::terrain_props;
        using namespace renegade::runtime;
        auto& scene = loaded.GetScene();
        auto start = ResolvePlayerStart(scene).start;
        start.transform.translation = {-6, .15f, -10};
        RuntimePlayerState player;
        RuntimePlayerViewRigState rig;
        std::string error;
        wi::physics::SetEnabled(true);
        wi::physics::SetSimulationEnabled(true);
        wi::physics::SetFrameRate(60);
        wi::physics::SetInterpolationEnabled(true);
        Check(SpawnRuntimePlayer(scene,start,player,error,start.settings),error);
        Check(SpawnRuntimePlayerViewRig(scene,rig,player.entity,start.settings.eyeHeight,error),error);
        const auto world = loaded.TerrainPropCollisions();
        Check(world && world->ResidentEnabled(),"Review terrain requires collision residency");
        // This measures steady walking over resident terrain, not cold-streaming
        // admission. Pin the radius-one fixture before movement prediction can
        // grow the actor's requests from four cells to six partway along the walk.
        constexpr std::uint64_t routeInterest = 0x475241535357414cULL;
        Check(world->SetInterest(routeInterest,{{0,0,0},{0,0,0},1},error),error);
        struct ScopedRouteInterest
        {
            std::shared_ptr<TerrainPropCollisionWorld> world;
            std::uint64_t id;
            ~ScopedRouteInterest() { world->RemoveInterest(id); }
        } route{world,routeInterest};
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(30);
        bool routeReady=false;
        unsigned preparationFrames=0;
        for (; preparationFrames<1800 && std::chrono::steady_clock::now()<deadline; ++preparationFrames)
        {
            routeReady=world->UpdateSceneInterests(0);
            if (routeReady && world->Ready()) break;
            // Native terrain publication still runs while simulation is held.
            // No input, synthetic ready flags or collision records are injected.
            scene.Update(0);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        const auto prepared=world->Stats();
        Check(routeReady && world->Ready() && prepared.terrainRequired==9 && prepared.terrainReady==9,
            "Walking route did not prepare all nine native collision tiles: "+prepared.diagnostic);
        std::cout<<"GRASS_WALK_RESIDENT_ROUTE required="<<prepared.terrainRequired
            <<" ready="<<prepared.terrainReady<<" preparation_frames="<<preparationFrames<<std::endl;
        wi::scene::CameraComponent camera;
        camera.CreatePerspective(960,640,.1f,500.f);
        constexpr float dt = 1.f / 75;
        float previousZ = start.transform.translation.z;
        float previousHandY = 0;
        for (int frame=0; frame<510; ++frame)
        {
            const bool ready = world->Ready();
            PlayerInputFrame input;
            input.moveForward = frame>=75 && ready ? 1.f : 0.f;
            (void)UpdateRuntimePlayer(scene,player,input,start.settings);
            (void)PoseRuntimePlayerViewRig(scene,rig,player.yaw,player.pitch);
            UpdateRuntimePlayerViewRigPresentation(scene,rig,input,ready ? dt : 0.f);
            const bool supported = world->UpdateSceneInterests(dt);
            scene.Update(supported ? dt : 0.f);
            ApplyRuntimePlayerCamera(scene,player,camera,start.settings);
            const float handY = scene.transforms.GetComponent(rig.primaryArmProof)->GetPosition().y-camera.Eye.y;
            if (previousZ >= -9 && previousZ < 14)
            {
                if (!supported || !ready)
                {
                    const auto stats = world->Stats();
                    std::cerr << "GRASS_WALK_COLLISION_NOT_READY frame=" << frame
                        << " z=" << camera.Eye.z << " previous_z=" << previousZ
                        << " ready_before=" << ready << " supported=" << supported
                        << " required=" << stats.terrainRequired << " resident=" << stats.terrainReady
                        << " diagnostic=" << stats.diagnostic << std::endl;
                    for (size_t i=0; i<scene.terrains.GetCount(); ++i)
                    {
                        const auto& t=scene.terrains[i];
                        std::cerr << "GRASS_WALK_TERRAIN busy=" << t.IsGenerationBusy() << std::endl;
                        for (const auto& record : native::GetTerrainChunkRecords(scene,t.terrainEntity))
                        {
                            const auto* body=scene.rigidbodies.GetComponent(record.entity);
                            std::cerr << "GRASS_WALK_CHUNK x=" << record.key.x << " z=" << record.key.z
                                << " valid=" << record.valid << " entity=" << record.entity
                                << " body=" << bool(body)
                                << " refresh=" << (body && body->IsRefreshParametersNeeded())
                                << " live=" << native::HasLiveBody(scene,record.entity) << std::endl;
                        }
                    }
                }
                Check(supported && ready,"Walking stopped for a resident terrain tile");
                Check(camera.Eye.z-previousZ > start.settings.walkSpeed*dt*.95f,
                    "Player slowed down on the flat route across the grass");
                Check(std::abs(handY-previousHandY)<.0025f,"Walking arms snapped relative to the camera");
            }
            previousZ = camera.Eye.z;
            previousHandY = handY;
        }
        Check(previousZ>14,"Character did not cross the grass onto bare terrain");

        // A previously live body cannot stand in for terrain currently being rebuilt.
        auto& terrain = scene.terrains[0];
        const auto pending = native::GetTerrainChunkRecords(scene,terrain.terrainEntity);
        Check(!pending.empty(),"Missing terrain collision records after the walk");
        wi::terrain::Chunk key;
        key.x = pending.front().key.x;
        key.z = pending.front().key.z;
        terrain.Generation_Cancel();
        auto& chunk = terrain.chunks.at(key);
        Check(native::HasLiveBody(scene,chunk.entity),"Stale-collision check requires a live old body");
        chunk.invalidated = true;
        Check(native::SetTerrainRequests(scene,terrain.terrainEntity,{},error),error);
        Check(!world->UpdateSceneInterests(0),"Invalidated terrain accepted stale collision");
        const auto held=world->Stats();
        Check(held.terrainRequired==9 && held.terrainReady==8 &&
            !native::GetTerrainChunk(scene,terrain.terrainEntity,pending.front().key).valid,
            "Invalidated route tile was not the sole blocked native collision cell");
        std::cout<<"GRASS_INVALIDATED_RESIDENT_TILE_HELD required="<<held.terrainRequired
            <<" ready="<<held.terrainReady<<std::endl;
        chunk.invalidated = false;
        terrain.Generation_Cancel();
        DespawnRuntimePlayerViewRig(scene,rig);
        DespawnRuntimePlayer(scene,player);
        std::cout<<"GRASS_STEADY_WALK_AND_ARM_POSE_PASS"<<std::endl;
    }

    inline void Run(wi::scene::Scene& scene, wi::terrain::Terrain& terrain, const fs::path& out,
        wi::RenderPath3D& render, wi::scene::CameraComponent& camera, bool hosted)
    {
        std::string error;
        const auto seed=(out/"terrain-template.wiscene").generic_u8string();
        { wi::Archive initial(seed,false,false); scene.Serialize(initial); Check(initial.SaveFile(seed),"Template save failed"); }
        ProjectService projects;
        projects.Initialize((out/"review-state.json").generic_u8string());
        Check(projects.CreateProject(out.generic_u8string(),"GrassReview",seed),projects.LastError());
        const auto& project=projects.CurrentProject();
        const auto root=fs::u8path(project.rootPath);
        Check(GrassPaletteEntries(root).empty() &&
            GrassPaletteEntries(root,PlantGroup::Grass).empty() &&
            GrassPaletteEntries(root,PlantGroup::Freshwater).empty() &&
            GrassPaletteEntries(root,PlantGroup::Marine).empty() &&
            GrassPaletteEntries(root,PlantGroup::Alien).empty(),
            "Retired stock plants remain discoverable");
        Check(fs::is_directory(root/CustomPlantDirectory), "New project is missing its Custom plant folder");
        GrassAtlas approved;
        Check(BuildStockGrassAtlas(root,StockGrassPreset::MeadowOpen,approved,error),error);
        Check(ImportGrassSheet(root,fs::u8path(approved.path),2,3,error),error);
        auto entries=GrassPaletteEntries(root,PlantGroup::Custom);
        Check(entries.size()==6,"Approved sheet did not import six custom cells");
        GrassAtlas complete,meadow,bank;
        Check(BuildGrassAtlas(root,entries,complete,error) && complete.frames==6,error);
        Check(BuildGrassAtlas(root,{entries[0],entries[1]},meadow,error),error);
        Check(BuildGrassAtlas(root,{entries[2],entries[3]},bank,error),error);
        Check(ImportGrassSheet(root,fs::u8path(meadow.path),2,2,error),error);
        auto imported=GrassPaletteEntries(root,PlantGroup::Custom);
        Check(imported.size()==8 && imported.front().parent_path()==root/CustomPlantDirectory,
            "Sheet import must publish its occupied cells under Custom");
        fs::copy_file(entries.front(),root/CustomPlantDirectory/"Dropped.PNG");
        fs::copy_file(entries.back(),(root/CustomPlantDirectory).parent_path()/"Legacy.png");
        Check(GrassPaletteEntries(root,PlantGroup::Custom).size()==10 && GrassPaletteEntries(root).size()==10,
            "Custom palette did not retain dropped PNGs or legacy project imports");
        Check(InitializeWickedVegetation(scene,terrain,&error),error);
        auto paint=[&](XMFLOAT3 centre, VegetationBrushMode mode, const GrassAtlas& atlas)
        {
            VegetationStrokeState before;
            auto result=PaintWickedVegetation(scene,terrain,centre,5.f,mode,&before,atlas);
            Check(result.changed && result.affectedVertices>0,"Grass stroke had no effect: "+result.error);
            FinalizeWickedVegetationStroke(scene,terrain,before);
            auto after=CaptureWickedVegetationAfter(scene,terrain,before);
            return WickedVegetationStrokeCommand(scene,terrain.terrainEntity,std::move(before),std::move(after));
        };
        const XMFLOAT3 left{-6,0,4},right{6,0,4};
        auto first=paint(left,VegetationBrushMode::Paint,meadow);
        const auto beforeBank=Capture(scene);
        auto second=paint(right,VegetationBrushMode::Paint,bank);
        const auto both=Capture(scene);
        Check(both.size()==2 && beforeBank.size()==1 && both.at(beforeBank.begin()->first)==beforeBank.begin()->second,
            "Riverbank changed meadow grass inside the same chunk");
        second.Undo(); Check(Capture(scene)==beforeBank,"Undo riverbank changed meadow");
        Check(second.Execute() && Capture(scene)==both,"Redo lost mixed grass");
        const auto settings=CaptureWickedVegetationSettings(terrain);
        auto edited=settings;edited.length=settings.length*.65f;
        Check(ApplyWickedVegetationSettings(scene,terrain,edited),"Grass length edit failed");
        for(size_t i=0;i<scene.hairs.GetCount();++i)
        {
            const auto* name=scene.names.GetComponent(scene.hairs.GetEntity(i));
            if(name && name->name=="Painted grass")
                Check(std::abs(scene.hairs[i].length-edited.length)<.0001f,"Appearance edit missed a palette layer");
        }
        Check(ApplyWickedVegetationSettings(scene,terrain,settings),"Grass length restore failed");
        auto erase=paint(left,VegetationBrushMode::Delete,{});
        Check(Capture(scene).size()==1,"Eraser left grass on cleared patch");
        erase.Undo(); Check(Capture(scene)==both,"Undo erase did not restore exact masks");
        camera.TransformCamera(DirectX::XMMatrixInverse(nullptr,DirectX::XMMatrixLookAtLH(
            DirectX::XMVectorSet(0,2,-8,1),DirectX::XMVectorSet(0,.65f,4,1),DirectX::XMVectorSet(0,1,0,0))));
        camera.UpdateCamera();
        auto frame=[&]{
            render.PreUpdate();render.Update(1.f/60.f);render.PreRender();render.Render();
            wi::graphics::GetDevice()->SubmitCommandLists();wi::renderer::UpdateGPUSuballocator();
        };
        for (int i = 0; i < (hosted ? 3 : 45); ++i) frame();
        wi::graphics::GetDevice()->WaitForGPU();
        Check(Capture(scene)==both,"Native terrain updates replaced custom hair");
        Check(wi::helper::saveTextureToFile(render.GetRenderResult3D(),(out/"meadow-riverbank.png").generic_u8string()),
            "Grass capture failed");
        auto geometry=[&](const GrassAtlas& atlas)
        {
            for (size_t i=0;i<scene.hairs.GetCount();++i)
            {
                const auto id=scene.hairs.GetEntity(i);
                const auto* material=scene.materials.GetComponent(id);
                if (!material || material->textures[wi::scene::MaterialComponent::BASECOLORMAP].name!=atlas.path) continue;
                auto& hair=scene.hairs[i]; const auto& view=hair.vb_pos[0];
                wi::vector<uint8_t> bytes;
                Check(wi::helper::saveBufferToMemory(hair.generalBuffer,bytes) && view.size &&
                    view.offset+view.size<=bytes.size(),"Unable to read native grass vertices");
                return std::vector<uint8_t>(bytes.begin()+view.offset,bytes.begin()+view.offset+view.size);
            }
            throw std::runtime_error("Missing grass renderer");
        };
        auto verifyMeadow = [&](wi::scene::Scene& target)
        {
            for (size_t i = 0; i < target.hairs.GetCount(); ++i)
            {
                const auto* m = target.materials.GetComponent(target.hairs.GetEntity(i));
                if (m && fs::u8path(m->textures[wi::scene::MaterialComponent::BASECOLORMAP].name).filename()
                        == fs::u8path(meadow.path).filename())
                    return VerifyMix(target.hairs[i]);
            }
            throw std::runtime_error("Missing mixed meadow");
        };
        const auto meadowFrames = verifyMeadow(scene);
        auto meadowRest=geometry(meadow), bankRest=geometry(bank);
        wi::ecs::Entity force=wi::ecs::INVALID_ENTITY;
        UpdateGrassInteraction(scene,force,left,{0,1,0});
        for (int i = 0; i < (hosted ? 3 : 20); ++i) frame();
        wi::graphics::GetDevice()->WaitForGPU();
        Check(geometry(meadow)!=meadowRest && geometry(bank)==bankRest,
            "Pointer force did not produce a localized change in grass vertices");
        // Saving while the pointer is held must serialize the authored hair only.
        TransformState spawn;
        spawn.translation={0,.1f,-11};
        CreatePlayerStartCommand start(scene,spawn);
        Check(start.Execute(),"Review Player Start failed");
        const auto preview=scene.Entity_CreateCamera("Grass review",1280,720);
        auto& pose=*scene.transforms.GetComponent(preview);
        pose.MatrixTransform(DirectX::XMMatrixInverse(nullptr,DirectX::XMMatrixLookAtLH(
            DirectX::XMVectorSet(0,2,-8,1),DirectX::XMVectorSet(0,.65f,4,1),DirectX::XMVectorSet(0,1,0,0))));
        pose.UpdateTransform();
        scene.cameras.GetComponent(preview)->TransformCamera(pose);
        scene.cameras.GetComponent(preview)->UpdateCamera();
        const auto path=(root/fs::u8path(project.startupScene)).generic_u8string();
        { wi::Archive archive(path,false,false);scene.Serialize(archive);Check(archive.SaveFile(path),"Save failed"); }
        ClearGrassInteraction(scene,force);
        SceneService cold; Check(cold.LoadScene(path),cold.LastError());
        Check(Capture(cold.GetScene())==both,"Cold reopen changed textures or masks");
        Check(cold.GetScene().forces.GetCount()==0,"Cursor force leaked into saved level");
        RuntimePlayerState player;
        auto startData=ResolvePlayerStart(scene).start;
        startData.transform.translation=right;
        Check(SpawnRuntimePlayer(scene,startData,player,error,startData.settings),error);
        const auto* collider=scene.colliders.GetComponent(player.entity);
        Check(collider && collider->IsGPUEnabled() && !collider->IsCPUEnabled(),"Missing native grass player collider");
        for (int i = 0; i < (hosted ? 3 : 25); ++i) frame();
        wi::graphics::GetDevice()->WaitForGPU();
        Check(geometry(bank)!=bankRest,"Player capsule left grass vertices unchanged");
        DespawnRuntimePlayer(scene,player);
        std::cout<<"GRASS_CURSOR_AND_PLAYER_GPU_VERTEX_RESPONSE_PASS"<<std::endl;

        render.scene=&cold.GetScene();
        for (int i = 0; i < (hosted ? 3 : 15); ++i) frame();
        wi::graphics::GetDevice()->WaitForGPU();
        Check(verifyMeadow(cold.GetScene()) == meadowFrames, "Reopened meadow changed per-strand varieties");
        render.scene=&scene;
        Check(Capture(cold.GetScene())==both,"First native updates after cold reopen lost a painted layer");
        auto largeMix = paint(left, VegetationBrushMode::Paint, complete);
        for (int i = 0; i < (hosted ? 3 : 20); ++i) frame();
        wi::graphics::GetDevice()->WaitForGPU();
        bool checkedAll = false;
        for (size_t i = 0; i < scene.hairs.GetCount(); ++i)
            if (scene.hairs[i].atlas_rects.size() == complete.frames)
            {
                VerifyMix(scene.hairs[i]);
                checkedAll = true;
            }
        Check(checkedAll, "Missing six-variety custom mix");
        largeMix.Undo();
        VerifyPlayerWalk(cold);
        stock_plant_profiles_proof::Run(scene,terrain,root,render,hosted);
        std::cout<<"GRASS_PALETTE_MIX_IMPORT_HISTORY_COLD_REOPEN_PASS"<<std::endl;
    }
}
