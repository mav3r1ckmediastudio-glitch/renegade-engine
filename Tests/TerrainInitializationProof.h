#pragma once
#include "renegade/bridge/VegetationService.h"
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <map>
#include <memory>
#include <stdexcept>
#include <vector>

namespace terrain_initialization_proof
{
    using Heights = std::map<std::pair<int,int>,std::vector<std::uint16_t>>;
    using Masks = std::map<std::pair<int,int>,std::vector<float>>;

    inline void Check(bool ok, const char* message)
    {
        if (!ok) throw std::runtime_error(message);
    }

    inline Heights Encoded(const wi::terrain::Terrain& terrain)
    {
        Heights result;
        for (const auto& entry : terrain.chunks)
            result[{entry.first.x,entry.first.z}] = entry.second.heightmap_data;
        return result;
    }

    inline Masks Grass(const wi::terrain::Terrain& terrain)
    {
        Masks result;
        for (const auto& entry : terrain.chunks)
            result[{entry.first.x,entry.first.z}] = entry.second.grass.vertex_lengths;
        return result;
    }

    template<class Frame>
    void Run(wi::scene::Scene& scene, wi::ecs::Entity terrainID,
             wi::RenderPath3D& render, const std::filesystem::path& out, Frame&& frame)
    {
        using namespace renegade::bridge;
        auto* terrain = scene.terrains.GetComponent(terrainID);
        auto* gpu = wi::graphics::GetDevice();
        std::size_t synchronizedChunks = static_cast<std::size_t>(-1);
        const auto advance = [&] {
            frame();
            if (render.scene == &scene && IsWickedVegetationInitialized(scene,*terrain) &&
                synchronizedChunks != terrain->chunks.size())
            {
                bool ready = false;
                SynchronizeWickedVegetation(scene,*terrain,ready);
                synchronizedChunks = ready ? terrain->chunks.size() : static_cast<std::size_t>(-1);
            }
        };
        const auto settle = [&] {
            for (int i=0;i<12;++i) advance();
            gpu->WaitForGPU();
            terrain->Generation_Cancel();
        };
        const auto equal = [&](TerrainSculptState expected, const char* why) {
            auto actual=CaptureTerrainSculpt(scene,*terrain);
            Check(RetainChangedTerrainSculpt(expected,actual)==0,why);
        };
        const auto minimum = [&] {
            float value=FLT_MAX;
            for (const auto& chunk : CaptureTerrainSculpt(scene,*terrain).chunks)
                for (float y : chunk.heights) value=std::min(value,y);
            return value;
        };
        const auto initial=CaptureTerrainSculpt(scene,*terrain);
        Check(SculptTerrain(scene,*terrain,{33,0,33},12,2,.5f,TerrainSculptMode::Lower,0),
            "Fresh terrain cannot sculpt below zero");
        auto lowered=CaptureTerrainSculpt(scene,*terrain);
        auto before=initial,after=lowered;
        Check(RetainChangedTerrainSculpt(before,after)>=4,"Lower stroke missed shared chunk corner");
        CommandService history;
        Check(history.RecordExecuted(std::make_unique<SculptTerrainCommand>(
            scene,terrainID,before,after)),"Lower stroke history missing");
        Check(minimum()<-1.99f,"Lower brush did not reach requested depth");
        Check(history.Undo(),"Lower Undo failed");
        equal(initial,"Lower Undo did not restore zero terrain");
        Check(history.Redo(),"Lower Redo failed");
        equal(lowered,"Lower Redo did not restore sculpt");
        std::cout<<"FRESH_LOWER_CORNER_UNDO_REDO_PASS"<<std::endl;

        // Extreme brush input must stop at the encoding envelope; clamping only
        // the saved R16 texture leaves different visible/physics and saved shapes.
        const auto beforeLimit=CaptureTerrainSculpt(scene,*terrain);
        Check(SculptTerrain(scene,*terrain,{180,0,180},4,1000,.5f,
            TerrainSculptMode::Lower,0),"Floor-limit stroke did not apply");
        Check(minimum()>=terrain->bottomLevel-.0001f,
            "Lower brush moved mesh below the encoding floor");
        Check(std::abs(minimum()-terrain->bottomLevel)<.0001f,
            "Lower brush did not stop at the floor");
        auto floorBefore=beforeLimit,floorAfter=CaptureTerrainSculpt(scene,*terrain);
        RetainChangedTerrainSculpt(floorBefore,floorAfter);
        SculptTerrainCommand floorHistory(scene,terrainID,floorBefore,floorAfter);
        floorHistory.Undo();
        equal(beforeLimit,"Floor-limit Undo did not restore sculpt");
        Check(SculptTerrain(scene,*terrain,{-180,0,-180},4,1000,.5f,
            TerrainSculptMode::Raise,0),"Ceiling-limit stroke did not apply");
        for (const auto& chunk : CaptureTerrainSculpt(scene,*terrain).chunks)
            for(float y:chunk.heights)
                Check(y<=terrain->topLevel+.0001f,"Raise brush exceeded encoding ceiling");
        auto ceilingBefore=beforeLimit,ceilingAfter=CaptureTerrainSculpt(scene,*terrain);
        RetainChangedTerrainSculpt(ceilingBefore,ceilingAfter);
        SculptTerrainCommand ceilingHistory(scene,terrainID,ceilingBefore,ceilingAfter);
        ceilingHistory.Undo();
        equal(beforeLimit,"Ceiling-limit Undo did not restore sculpt");
        std::cout<<"SCULPT_ENCODING_LIMITS_AND_RESTORE_PASS"<<std::endl;

        std::string error;
        Check(InitializeWickedVegetation(scene,*terrain,&error),error.c_str());
        Check(terrain->IsGrassEnabled(),"Manual grass painting did not enable native grass");
        VegetationStrokeState grassBefore;
        const auto painted=PaintWickedVegetation(scene,*terrain,{250,0,250},8,
            VegetationBrushMode::Paint,&grassBefore);
        Check(painted.changed && painted.affectedVertices>0,"Manual grass cannot paint new terrain");
        FinalizeWickedVegetationStroke(scene,*terrain,grassBefore);
        const auto grassAfter=CaptureWickedVegetationAfter(*terrain,grassBefore);
        WickedVegetationStrokeCommand grassHistory(scene,terrainID,grassBefore,grassAfter);
        const auto paintedMasks=Grass(*terrain);
        grassHistory.Undo();
        Check(Grass(*terrain)!=paintedMasks,"Grass Undo did not remove painted masks");
        Check(grassHistory.Execute(),"Grass Redo failed");
        Check(Grass(*terrain)==paintedMasks,"Grass Redo changed painted masks");
        settle();

        // Every resident physics heightfield must use the generated local zero,
        // and completed sculpt strokes must rebuild its true contact geometry.
        RefreshTerrainSculptPhysics(scene,*terrain,after);
        wi::physics::SetEnabled(true);
        wi::physics::SetSimulationEnabled(false);
        settle();
        for (const auto sample : {XMFLOAT3(0,100,0),XMFLOAT3(33,100,33)})
        {
            const wi::primitive::Ray ray(sample,{0,-1,0},0,200);
            const auto visual=wi::scene::Pick(ray,wi::enums::FILTER_TERRAIN,~0u,scene);
            const auto contact=wi::physics::Intersects(scene,ray);
            Check(visual.entity!=wi::ecs::INVALID_ENTITY && contact.IsValid() &&
                std::abs(visual.position.y-contact.position.y)<.02f,
                "Jolt contact differs from generated/sculpted surface");
        }
        std::cout<<"GENERATED_AND_SCULPTED_JOLT_CONTACT_PASS"<<std::endl;

        const auto inner=CaptureTerrainSculpt(scene,*terrain);
        const auto encodedInner=Encoded(*terrain);
        ExpandTerrainCommand expand(scene,terrainID);
        const auto waitFor = [&](std::size_t count) {
            for(int i=0;i<2400 && (terrain->chunks.size()!=count || terrain->IsGenerationBusy());++i)
            {
                advance();
                Sleep(5);
            }
            settle();
            Check(terrain->chunks.size()==count,"Expansion did not generate expected chunk count");
        };
        const auto verifyRing = [&](int radius) {
            equal(inner,"Expansion changed inner sculpted chunks");
            const auto encodedNow=Encoded(*terrain);
            for(const auto& old:encodedInner)
                Check(encodedNow.at(old.first)==old.second,"Expansion changed inner R16 heights");
            for(const auto& entry:terrain->chunks)
            {
                if(std::max(std::abs(entry.first.x),std::abs(entry.first.z))!=radius) continue;
                const auto* mesh=scene.meshes.GetComponent(entry.second.entity);
                Check(mesh!=nullptr,"New ring mesh missing");
                for(const auto& vertex:mesh->vertex_positions)
                    Check(std::abs(vertex.y)<.003f,"Expansion created a floor-height cliff");
                for(float length:entry.second.grass.vertex_lengths)
                    Check(length==0,"Expansion invented painted grass");
            }
        };
        Check(expand.Execute(),"Fresh unlinked terrain expansion refused");
        waitFor(441);
        verifyRing(10);
        expand.Undo();
        settle();
        Check(terrain->chunks.size()==361,"Expansion Undo retained the outer ring");
        equal(inner,"Expansion Undo changed inner sculpt");
        Check(expand.Execute(),"Expansion Redo refused");
        waitFor(441);
        verifyRing(10);
        std::cout<<"FRESH_EXPANSION_NO_CLIFF_INNER_PRESERVED_UNDO_REDO_PASS"<<std::endl;

        const auto savedHeights=Encoded(*terrain);
        const auto savedGrass=Grass(*terrain);
        const auto path=(out/"expanded-sculpted-painted.wiscene").generic_u8string();
        wi::Archive disk(path,false,false);
        scene.Serialize(disk);
        Check(disk.SaveFile(path),"Expanded terrain save failed");
        SceneService cold;
        Check(cold.LoadScene(path),"Expanded terrain cold hydrate failed");
        auto& reopened=cold.GetScene();
        Check(reopened.terrains.GetCount()==1,"Expanded terrain disappeared on reopen");
        auto& loaded=reopened.terrains[0];
        Check(loaded.chunks.size()==441 && Encoded(loaded)==savedHeights &&
            Grass(loaded)==savedGrass,"Cold reopen changed sculpt, extent or grass");
        const auto loadedID=reopened.terrains.GetEntity(0);
        Check(std::abs(reopened.transforms.GetComponent(loadedID)->translation_local.y)<.0001f,
            "Cold reopen shifted terrain root");
        Check(loaded.modifiers.size()==1,"Cold reopen lost the initial surface generator");
        float normalized=0;
        loaded.modifiers[0]->Apply({10000,-10000},normalized);
        Check(std::abs(loaded.bottomLevel+normalized*(loaded.topLevel-loaded.bottomLevel))<.0001f,
            "Serialized generator no longer targets zero");
        auto* previousScene=render.scene;
        struct RestoreScene
        {
            wi::RenderPath3D& render;
            wi::scene::Scene* previous;
            ~RestoreScene() { render.scene=previous; }
        } restore{render,previousScene};
        render.scene=&reopened;
        for(int i=0;i<12;++i) frame();
        gpu->WaitForGPU();loaded.Generation_Cancel();
        Check(Encoded(loaded)==savedHeights && Grass(loaded)==savedGrass,
            "First native updates after reopen changed authored terrain");
        const auto contact=wi::physics::Intersects(reopened,
            wi::primitive::Ray(XMFLOAT3(33,100,33),XMFLOAT3(0,-1,0),0,200));
        Check(contact.IsValid() && std::abs(contact.position.y+2.f)<.02f,
            "Cold-reopened sculpt lost its collision surface");
        render.scene=previousScene;
        gpu->WaitForGPU();
        std::cout<<"EXPANDED_COLD_REOPEN_GENERATOR_GRASS_COLLISION_PASS"<<std::endl;
    }
}
