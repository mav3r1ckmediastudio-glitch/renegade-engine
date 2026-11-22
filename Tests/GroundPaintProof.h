#pragma once
#include "renegade/bridge/GroundPaint.h"

namespace ground_paint_proof
{
    using namespace renegade::bridge;
    namespace fs=std::filesystem;
    inline void Run(wi::scene::Scene& scene,wi::terrain::Terrain& terrain,const fs::path& out,
        wi::RenderPath3D& render,wi::scene::CameraComponent& camera)
    {
        auto check=[](bool ok,const std::string& message){if(!ok)throw std::runtime_error(message);};
        const auto seed=(out/"seed.wiscene").generic_u8string();
        {wi::Archive a(seed,false,false);scene.Serialize(a);check(a.SaveFile(seed),"Seed save failed");}
        ProjectService projects;projects.Initialize((out/"review.json").generic_u8string());
        check(projects.CreateProject(out.generic_u8string(),"TerrainPaintReview",seed),projects.LastError());
        const auto project=projects.CurrentProject();auto root=fs::u8path(project.rootPath);
        auto choices=GroundPalette(root);check(choices.size()==10,"Expected ten ground materials");
        std::string error;
        for(const auto& m:choices)check(fs::is_regular_file(m.colour) && fs::is_regular_file(m.normal),"Stock ground maps missing: "+m.name);
        auto gravel=choices.back();GroundMaterial custom;
        check(ImportGroundMaterial(root,gravel.colour,custom,error),error);
        check(!custom.normal.empty() && !custom.surface.empty() && GroundPalette(root).size()==11,"Custom PBR import lost maps");
        auto id=[&](const GroundMaterial& m){auto e=ResolveGroundMaterial(scene,m,root,error);check(e!=wi::ecs::INVALID_ENTITY,error);return e;};
        const auto meadow=id(choices[0]),dirt=id(choices[2]),stone=id(choices[4]),stones=id(gravel);
        auto base=SetGroundBase(scene,terrain.terrainEntity,meadow,stone,{});
        check(base->Execute(),"Set as Base failed");
        const auto flat=CaptureTerrainSculpt(scene,terrain);
        check(SculptTerrain(scene,terrain,{11,0,15},10,11,.2f,TerrainSculptMode::Raise,0),"Bank sculpt failed");
        size_t cliff=0;for(const auto& entry:terrain.chunks)
            cliff+=std::count_if(entry.second.blendmap_layers[1].pixels.begin(),entry.second.blendmap_layers[1].pixels.end(),[](uint8_t v){return v>80;});
        check(cliff>0,"Sculpted bank did not expose rock");
        check(ApplyTerrainSculpt(scene,terrain,flat),"Flat terrain restore failed");
        for(const auto& entry:terrain.chunks)
            check(std::none_of(entry.second.blendmap_layers[1].pixels.begin(),entry.second.blendmap_layers[1].pixels.end(),[](uint8_t v){return v>0;}),"Flattened bank retained rock");
        check(SculptTerrain(scene,terrain,{11,0,15},10,11,.2f,TerrainSculptMode::Raise,0),"Review bank failed");
        auto vegetation=GrassPaletteEntries(root,PlantGroup::Grass);
        GrassAtlas atlas;check(BuildGrassAtlas(root,{vegetation.front()},atlas,error),error);
        check(InitializeWickedVegetation(scene,terrain,&error),error);
        auto appearance=CaptureWickedVegetationSettings(terrain);
        appearance.length*=.4f;appearance.width*=.65f;
        (void)ApplyWickedVegetationSettings(scene,terrain,appearance);
        VegetationStrokeState grass;
        auto painted=PaintWickedVegetation(scene,terrain,{0,0,0},17,VegetationBrushMode::Paint,&grass,atlas);
        check(painted.changed,"No test grass painted");FinalizeWickedVegetationStroke(scene,terrain,grass);
        auto live=[&]{size_t n=0;for(size_t i=0;i<scene.hairs.GetCount();++i)
            {auto* label=scene.names.GetComponent(scene.hairs.GetEntity(i));if(label && label->name=="Painted grass")n+=std::count_if(scene.hairs[i].vertex_lengths.begin(),scene.hairs[i].vertex_lengths.end(),[](float v){return v>0;});}return n;};
        const auto originalGrass=live();check(originalGrass>0,"No initial live grass");
        GroundStroke road(scene,terrain.terrainEntity,dirt,GroundTool::Paint,5,1,.65f);
        check(road.Dab({-43,0,-1}) && road.Dab({-5,0,0}) && road.Dab({42,0,6}),"Cross-chunk paint failed");
        wi::terrain::Chunk key{};auto& c=terrain.chunks.at(key);
        const auto earth=c.blendmap_layers.back().pixels;
        check(std::any_of(earth.begin(),earth.end(),[](auto v){return v>0 && v<255;}),"No soft edge");
        const auto afterDirt=live();check(afterDirt<originalGrass,"Earth did not thin grass");
        GroundStroke top(scene,terrain.terrainEntity,stones,GroundTool::Paint,2.75f,1,.55f);
        check(top.Dab({-43,0,-1}) && top.Dab({-5,0,0}) && top.Dab({42,0,6}),"Gravel stroke failed");
        const auto gravelMask=c.blendmap_layers.back().pixels;check(c.blendmap_layers[c.blendmap_layers.size()-2].pixels==earth,"Buried dirt changed");
        const auto gravelGrass=live();
        auto coverage=SetGroundSettings(scene,terrain.terrainEntity,stones,1.75f,1);
        check(coverage->Execute() && live()>gravelGrass,"Grass Coverage did not update painted gravel");
        coverage->Undo();check(live()==gravelGrass,"Undo Coverage did not restore exact grass");
        wi::terrain::Chunk neighbour{};neighbour.x=1;
        for(int z=0;z<wi::terrain::chunk_width;++z)
        {
            int right=(z+1)*wi::terrain::chunk_width-1,left=z*wi::terrain::chunk_width;
            check(c.blendmap_layers.back().pixels[right]==terrain.chunks.at(neighbour).blendmap_layers.back().pixels[left],"Chunk boundary split");
        }
        GroundStroke erase(scene,terrain.terrainEntity,stones,GroundTool::Erase,80,1,0);
        check(erase.Dab({0,0,0}) && c.blendmap_layers[c.blendmap_layers.size()-2].pixels==earth &&
            std::count(c.blendmap_layers.back().pixels.begin(),c.blendmap_layers.back().pixels.end(),0)==wi::terrain::vertexCount &&
            live()==afterDirt,"Erasing gravel failed to restore underlying surface and grass");
        check(erase.TryUndo() && c.blendmap_layers.back().pixels==gravelMask && live()==gravelGrass,"Undo failed");
        check(erase.Execute() && erase.TryUndo(),"Redo failed");
        GroundStroke soften(scene,terrain.terrainEntity,stones,GroundTool::Soften,7,1,.4f);
        check(soften.Dab({0,0,0}) && c.blendmap_layers.back().pixels!=gravelMask && soften.TryUndo(),"Local soften failed");
        check(top.TryUndo() && road.TryUndo() && live()==originalGrass,"Original plant positions lost");
        check(road.Execute() && top.Execute(),"Road redo failed");
        auto cameraPose=DirectX::XMMatrixInverse(nullptr,DirectX::XMMatrixLookAtLH(
            DirectX::XMVectorSet(-10,7,-13,1),DirectX::XMVectorSet(5,1.4f,5,1),DirectX::XMVectorSet(0,1,0,0)));
        camera.TransformCamera(cameraPose);camera.UpdateCamera();
        auto frame=[&]{render.PreUpdate();render.Update(1.f/60.f);render.PreRender();render.Render();
            wi::graphics::GetDevice()->SubmitCommandLists();wi::renderer::UpdateGPUSuballocator();};
        for(int i=0;i<45;++i)frame();wi::graphics::GetDevice()->WaitForGPU();terrain.Generation_Cancel();
        check(wi::helper::saveTextureToFile(render.GetRenderResult3D(),(out/"ground-paint.png").generic_u8string()),"Capture failed");
        const auto savedSize=scene.materials.GetComponent(stones)->texMulAdd.x;
        check(savedSize==gravel.tileMetres,"Gravel tiling did not retain metres per repeat");
        TransformState start;start.translation={0,1,-9};CreatePlayerStartCommand spawn(scene,start);check(spawn.Execute(),"Spawn failed");
        const auto path=(root/fs::u8path(project.startupScene)).generic_u8string();
        {wi::Archive a(path,false,false);scene.Serialize(a);check(a.SaveFile(path),"Scene save failed");}
        SceneService cold;check(cold.LoadScene(path),cold.LastError());auto& newScene=cold.GetScene();render.scene=&newScene;
        for(int i=0;i<20;++i)frame();wi::graphics::GetDevice()->WaitForGPU();
        auto& t=newScene.terrains[0];t.Generation_Cancel();
        check(t.chunks.at(key).blendmap_layers.back().pixels==gravelMask &&
            t.chunks.at(key).blendmap_layers[t.materialEntities.size()-2].pixels==earth,"Cold reopen changed paint");
        render.scene=&scene;std::cout<<"GROUND_PAINT_IMPORT_SOFT_EDGE_ERASE_GRASS_HISTORY_REOPEN_PASS"<<std::endl;
    }
}
