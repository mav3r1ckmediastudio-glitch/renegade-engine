#pragma once
#include "TerrainInitializationProof.h"
#include "../WickedEngine/Editor/json.hpp"
#include "renegade/bridge/RiverbedMaterialService.h"

namespace maximum_terrain_river_proof
{
    inline auto PaintMasks(const wi::terrain::Terrain& terrain)
    {
        std::map<std::pair<int,int>,std::vector<std::uint8_t>> result;
        for(const auto& item:terrain.chunks)
        {
            auto& values=result[{item.first.x,item.first.z}];
            for(const auto& layer:item.second.blendmap_layers)
                values.insert(values.end(),layer.pixels.begin(),layer.pixels.end());
        }
        return result;
    }
    template<class Update,class Draw>
    void Run(wi::scene::Scene& scene,wi::ecs::Entity terrainID,
        wi::RenderPath3D& render,const std::filesystem::path& out,
        bool linkFirst,bool boundedDraws,Update&& update,Draw&& draw)
    {
        using namespace renegade::bridge;
        using terrain_initialization_proof::Check;
        using terrain_initialization_proof::Encoded;
        using terrain_initialization_proof::Grass;
        auto* terrain=scene.terrains.GetComponent(terrainID);
        auto* gpu=wi::graphics::GetDevice();
        std::string error;
        const auto started=std::chrono::steady_clock::now();
        const auto stage=[&](const char* name) {
            std::cout<<"MAXIMUM_STAGE "<<name<<" elapsed_ms="
                <<std::chrono::duration<double,std::milli>(
                    std::chrono::steady_clock::now()-started).count()<<std::endl;
        };
        const auto settle=[&](std::size_t count) {
            const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(60);
            for(;;)
            {
                update(); Sleep(1);
                if(terrain->chunks.size()==count && !terrain->IsGenerationBusy())
                {
                    bool ready=true;
                    for(const auto& item:terrain->chunks)
                        if(item.second.invalidated || !scene.meshes.Contains(item.second.entity))
                            {ready=false;break;}
                    if(ready)break;
                }
                Check(std::chrono::steady_clock::now()<deadline,
                    "Maximum terrain did not publish the full native ring");
            }
            for(int i=0;i<2;++i)update();
            gpu->WaitForGPU();terrain->Generation_Cancel();
            Check(terrain->chunks.size()==count,"Unexpected resident terrain extent");
        };
        const auto same=[&](TerrainSculptState expected,const char* why) {
            auto actual=CaptureTerrainSculpt(scene,*terrain);
            Check(actual.chunks.size()==expected.chunks.size(),why);
            Check(RetainChangedTerrainSculpt(expected,actual)==0,why);
        };
        const auto river=[&](float z) {
            RiverSplineOptions options;
            CreateRiverSplineCommand create(scene,
                {{-1030,0,z},{-500,0,z},{0,0,z},{500,0,z},{1030,0,z}},options);
            Check(create.Execute(),"Maximum terrain river creation failed");
            update();gpu->WaitForGPU();terrain->Generation_Cancel();
            return create.CreatedEntity();
        };
        const auto link=[&](wi::ecs::Entity id) {
            SetRiverTerrainLayerCommand command(scene,terrainID,id,1.35f,2.f,true);
            if(!command.Execute())throw std::runtime_error("Maximum terrain link: "+command.Error());
        };
        const auto mainRiver=river(0);
        if(linkFirst)
        {
            link(mainRiver);
            PaintRockyRiverbedCommand paint(scene,terrainID,mainRiver,
                std::filesystem::absolute("Content/terrain/riverbed/rocky_rugged").generic_u8string());
            if(!paint.Execute())throw std::runtime_error(paint.Error());
        }
        CommandService history;
        for(int radius=DefaultTerrainChunkRadius+1;radius<=MaximumTerrainChunkRadius;++radius)
        {
            const auto before=Encoded(*terrain);
            const auto paintBefore=PaintMasks(*terrain);
            auto report=std::make_shared<std::string>();
            Check(history.Execute(std::make_unique<ExpandTerrainCommand>(scene,terrainID,report)),
                report->empty()?"Maximum expansion command rejected":report->c_str());
            const auto side=std::size_t(TerrainChunkCountPerSide(radius));
            settle(side*side);
            const auto after=Encoded(*terrain);
            const auto paintAfter=PaintMasks(*terrain);
            for(const auto& item:paintBefore)
                Check(paintAfter.at(item.first)==item.second,"Expansion changed existing material paint");
            for(const auto& item:before)
                Check(after.at(item.first)==item.second,"Expansion changed an existing riverbed");
            for(const auto& item:terrain->chunks)
            {
                Check(std::abs(item.first.x)<=radius && std::abs(item.first.z)<=radius,
                    "Expansion published an out-of-bounds chunk");
                if(std::max(std::abs(item.first.x),std::abs(item.first.z))!=radius)continue;
                const auto* mesh=scene.meshes.GetComponent(item.second.entity);
                Check(mesh!=nullptr,"Outer ring mesh missing");
                for(const auto& vertex:mesh->vertex_positions)
                    Check(vertex.y>=-2.f && vertex.y<.003f,
                        "Expansion produced a cliff or invalid riverbed depth");
            }
            if(linkFirst)
                Check(ValidateRiverTerrainLayers(scene,terrainID,error),error.c_str());
            std::cout<<"MAXIMUM_RING_PASS radius="<<radius<<" chunks="<<side*side
                <<" linked_first="<<linkFirst<<std::endl;
        }
        Check(terrain->generation==16 && terrain->chunks.size()==1089,
            "Maximum-size proof did not reach the supported 33x33 extent");
        ExpandTerrainCommand beyond(scene,terrainID);
        const auto atLimit=Encoded(*terrain);
        Check(!beyond.Execute() && Encoded(*terrain)==atLimit,
            "Expansion exceeded its supported maximum or altered terrain on refusal");
        std::vector<std::unique_ptr<SetRiverTerrainLayerCommand>> laterLinks;
        const auto laterLink=[&](wi::ecs::Entity id) {
            auto command=std::make_unique<SetRiverTerrainLayerCommand>(scene,terrainID,id,1.35f,2.f,true);
            if(!command->Execute())throw std::runtime_error(command->Error());
            laterLinks.push_back(std::move(command));
        };
        if(!linkFirst)laterLink(mainRiver);
        laterLink(river(-400));laterLink(river(400));
        auto linked=CaptureTerrainSculpt(scene,*terrain);
        const auto document=nlohmann::json::parse(linked.riverLayerDocument);
        Check(document.at("version").get<int>()==3,
            "Maximum terrain did not use bounded sparse sample storage");
        Check(document.at("chunks").size()>81,
            "Maximum proof did not cross the historical 81 edited-chunk boundary");
        Check(linked.riverLayerDocument.size()<32*1024*1024,
            "Maximum terrain exceeded its unchanged payload limit");
        Check(terrain->bottomLevel==-20.f && terrain->topLevel==120.f,
            "Maximum terrain needed Allow Deeper Beds");
        std::cout<<"MAXIMUM_THREE_RIVERS_DEFAULT_DEPTH_PASS patches="
            <<document.at("chunks").size()<<" bytes="<<linked.riverLayerDocument.size()<<std::endl;

        // Malformed v3 and over-budget patches must fail before changing heights.
        const auto validPayload=linked.riverLayerDocument;
        const auto validEncoding=Encoded(*terrain);
        auto* metadata=scene.metadatas.GetComponent(terrainID);
        auto malformed=document;
        malformed["chunks"][0]["indices"][0]=wi::terrain::vertexCount;
        metadata->string_values.set(RiverTerrainLayersMetadataKey,malformed.dump());
        Check(!RecomposeRiverTerrainLayers(scene,terrainID,error) &&
            Encoded(*terrain)==validEncoding,"Invalid sparse patch changed maximum terrain");
        malformed=document;
        const std::size_t excess=81*wi::terrain::vertexCount+1;
        malformed["chunks"][0]["indices"]=std::vector<std::size_t>(excess,0);
        malformed["chunks"][0]["base"]=std::vector<float>(excess,0.f);
        malformed["chunks"][0]["last"]=std::vector<float>(excess,-1.f);
        metadata->string_values.set(RiverTerrainLayersMetadataKey,malformed.dump());
        Check(!RecomposeRiverTerrainLayers(scene,terrainID,error) &&
            Encoded(*terrain)==validEncoding,"Over-budget sparse patch changed maximum terrain");
        metadata->string_values.set(RiverTerrainLayersMetadataKey,validPayload);
        Check(ValidateRiverTerrainLayers(scene,terrainID,error),error.c_str());
        std::cout<<"MAXIMUM_SPARSE_PATCH_VALIDATION_AND_BUDGET_FAIL_CLOSED_PASS"<<std::endl;

        auto node=scene.splines.GetComponent(mainRiver)->spline_node_entities[3];
        auto pose=CaptureTransform(*scene.transforms.GetComponent(node));
        auto changed=pose;changed.scale.x*=1.5f;
        SetWatercourseTransformCommand width(scene,node,pose,changed);
        Check(width.Execute(),"Maximum terrain linked width edit failed");
        width.Undo();same(linked,"Maximum terrain width Undo changed riverbed");
        Check(width.Execute(),"Maximum terrain width Redo failed");width.Undo();
        auto riverPose=CaptureTransform(*scene.transforms.GetComponent(mainRiver));
        auto moved=riverPose;moved.translation.z=70;
        SetWatercourseTransformCommand move(scene,mainRiver,riverPose,moved);
        Check(move.Execute(),"Maximum terrain linked river move failed");
        move.Undo();same(linked,"Maximum terrain river move Undo changed riverbed");
        Check(move.Execute(),"Maximum terrain river move Redo failed");move.Undo();

        // Undo subsequent sculpt and layer commands in order, then cross the
        // last expansion and redo them. Outer chunk entity targets must survive.
        const auto beforeSculpt=CaptureTerrainSculpt(scene,*terrain);
        Check(SculptTerrain(scene,*terrain,{1056,0,750},7,1.f,.5f,TerrainSculptMode::Lower,0),
            "Maximum outer-ring sculpt failed");
        auto sculpted=CaptureTerrainSculpt(scene,*terrain);
        auto oldSculpt=beforeSculpt,newSculpt=sculpted;
        Check(RetainChangedTerrainSculpt(oldSculpt,newSculpt)>0,"Outer sculpt changed no samples");
        SculptTerrainCommand sculpt(scene,terrainID,oldSculpt,newSculpt);
        sculpt.Undo();same(beforeSculpt,"Maximum sculpt Undo failed");
        const auto laterState=CaptureTerrainSculpt(scene,*terrain);
        for(auto it=laterLinks.rbegin();it!=laterLinks.rend();++it)(*it)->Undo();
        Check(history.Undo(),"Maximum expansion Undo failed");
        settle(961);
        Check(history.Redo(),"Maximum expansion Redo failed");
        settle(1089);
        for(auto& command:laterLinks)Check(command->Execute(),"Maximum later river Redo failed");
        same(laterState,"Expansion round-trip lost maximum-size river state");
        Check(sculpt.Execute(),"Outer sculpt Redo lost its restored chunk entity");
        same(sculpted,"Outer sculpt Redo changed after expansion round-trip");
        Check(ValidateRiverTerrainLayers(scene,terrainID,error),error.c_str());
        std::cout<<"MAXIMUM_LINKED_EXPANSION_SCULPT_HISTORY_PASS"<<std::endl;

        stage("rocky-paint");
        const auto beforePaint=PaintMasks(*terrain);
        PaintRockyRiverbedCommand rock(scene,terrainID,mainRiver,
            std::filesystem::absolute("Content/terrain/riverbed/rocky_rugged").generic_u8string());
        if(!rock.Execute())throw std::runtime_error(rock.Error());
        const auto rockyMasks=PaintMasks(*terrain);
        Check(rockyMasks!=beforePaint,"Maximum terrain rocky paint changed no masks");
        rock.Undo();Check(PaintMasks(*terrain)==beforePaint,"Maximum rocky paint Undo failed");
        Check(rock.Execute() && PaintMasks(*terrain)==rockyMasks,"Maximum rocky paint Redo failed");
        same(sculpted,"Maximum rocky paint changed sculpt or linked beds");
        stage("grass-paint");
        Check(InitializeWickedVegetation(scene,*terrain,&error),error.c_str());
        VegetationStrokeState grassBefore;
        auto painted=PaintWickedVegetation(scene,*terrain,{1056,0,750},8,
            VegetationBrushMode::Paint,&grassBefore);
        Check(painted.changed && painted.affectedVertices>0,"Maximum outer-ring grass paint failed");
        FinalizeWickedVegetationStroke(scene,*terrain,grassBefore);
        auto grassAfter=CaptureWickedVegetationAfter(*terrain,grassBefore);
        const auto masks=Grass(*terrain);
        WickedVegetationStrokeCommand grass(scene,terrainID,grassBefore,grassAfter);
        grass.Undo();Check(Grass(*terrain)!=masks,"Maximum grass Undo failed");
        Check(grass.Execute() && Grass(*terrain)==masks,"Maximum grass Redo failed");
        stage("collisions");
        wi::physics::SetEnabled(true);wi::physics::SetSimulationEnabled(false);
        for(int i=0;i<12;++i)update();
        gpu->WaitForGPU();terrain->Generation_Cancel();
        for(const auto sample:{XMFLOAT3(0,100,0),XMFLOAT3(1000,100,0),XMFLOAT3(1056,100,750)})
        {
            const wi::primitive::Ray ray(sample,{0,-1,0},0,200);
            auto visual=wi::scene::Pick(ray,wi::enums::FILTER_TERRAIN,~0u,scene);
            auto contact=wi::physics::Intersects(scene,ray);
            Check(visual.entity!=wi::ecs::INVALID_ENTITY && contact.IsValid() &&
                std::abs(visual.position.y-contact.position.y)<.02f,
                "Maximum terrain collision differs from the authored surface");
        }
        stage("source-render");
        // One real source capture is sufficient on hosted software graphics.
        // Keep the full terrain, source resolution and subsequent preview draw;
        // avoid rasterizing two additional identical copies of the source.
        const int sourceDraws=boundedDraws ? (linkFirst?0:1) : 3;
        for(int i=0;i<sourceDraws;++i)draw();
        gpu->WaitForGPU();terrain->Generation_Cancel();
        if(sourceDraws>0)
            Check(wi::helper::saveTextureToFile(render.GetRenderResult3D(),
                (out/"maximum-linked-rivers.png").generic_u8string()),"Maximum terrain GPU capture failed");
        std::cout<<"MAXIMUM_SOURCE_GPU_DRAWS="<<sourceDraws<<std::endl;
        const auto heights=Encoded(*terrain);
        const auto savedMasks=Grass(*terrain);
        const auto savedPaint=PaintMasks(*terrain);
        stage("save-cold-reopen");
        const auto path=(out/"maximum-linked-rivers.wiscene").generic_u8string();
        wi::Archive disk(path,false,false);scene.Serialize(disk);
        Check(disk.SaveFile(path),"Maximum terrain save failed");
        SceneService cold;Check(cold.LoadScene(path),"Maximum terrain cold hydration failed");
        auto& reopened=cold.GetScene();
        Check(reopened.terrains.GetCount()==1,"Maximum terrain missing after reopen");
        auto& loaded=reopened.terrains[0];auto loadedID=reopened.terrains.GetEntity(0);
        Check(loaded.generation==16 && loaded.chunks.size()==1089 &&
            Encoded(loaded)==heights && Grass(loaded)==savedMasks && PaintMasks(loaded)==savedPaint,
            "Maximum terrain cold reopen lost heights, extent or grass");
        Check(RecomposeRiverTerrainLayers(reopened,loadedID,error),error.c_str());
        auto* previous=render.scene;render.scene=&reopened;
        struct Restore {wi::RenderPath3D& render;wi::scene::Scene* scene;
            ~Restore(){render.scene=scene;}} restore{render,previous};
        for(int i=0;i<12;++i)update();
        gpu->WaitForGPU();loaded.Generation_Cancel();
        Check(Encoded(loaded)==heights && Grass(loaded)==savedMasks,
            "First native updates after maximum reopen changed authored content");
        auto contact=wi::physics::Intersects(reopened,
            wi::primitive::Ray(XMFLOAT3(1000,100,0),XMFLOAT3(0,-1,0),0,200));
        Check(contact.IsValid() && contact.position.y<-.75f,
            "Maximum cold-reopened outer river lost its collision bed");
        render.scene=previous;gpu->WaitForGPU();
        std::cout<<"MAXIMUM_1089_RIVERS_SCULPT_GRASS_HISTORY_COLLISION_COLD_REOPEN_PASS"
            <<" linked_first="<<linkFirst<<std::endl;
    }
}
