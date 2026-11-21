#pragma once
#include "renegade/bridge/VegetationService.h"
#include "renegade/bridge/SceneService.h"
#include "renegade/bridge/PlayerService.h"
#include "renegade/bridge/ProjectService.h"
#include <filesystem>
#include <map>

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

    inline void Run(wi::scene::Scene& scene, wi::terrain::Terrain& terrain, const fs::path& out,
        wi::RenderPath3D& render, wi::scene::CameraComponent& camera)
    {
        std::string error;
        const auto seed=(out/"terrain-template.wiscene").generic_u8string();
        { wi::Archive initial(seed,false,false); scene.Serialize(initial); Check(initial.SaveFile(seed),"Template save failed"); }
        ProjectService projects;
        projects.Initialize((out/"review-state.json").generic_u8string());
        Check(projects.CreateProject(out.generic_u8string(),"GrassReview",seed),projects.LastError());
        const auto& project=projects.CurrentProject();
        const auto root=fs::u8path(project.rootPath);
        auto entries=GrassPaletteEntries(root);
        Check(entries.size()==10,"Ten bundled grass cutouts required");
        GrassAtlas meadow,bank;
        Check(BuildGrassAtlas(root,{entries[0],entries[5]},meadow,error),error);
        Check(BuildGrassAtlas(root,{entries[1],entries[7]},bank,error),error);
        Check(ImportGrassSheet(root,fs::u8path(meadow.path),2,2,error),error);
        Check(GrassPaletteEntries(root).size()==12,"Sheet import must keep both occupied cells and omit transparent padding");
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
        for(int i=0;i<45;++i)frame();
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
        auto meadowRest=geometry(meadow), bankRest=geometry(bank);
        wi::ecs::Entity force=wi::ecs::INVALID_ENTITY;
        UpdateGrassInteraction(scene,force,left,{0,1,0});
        for(int i=0;i<20;++i)frame();
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
        for(int i=0;i<25;++i)frame();
        wi::graphics::GetDevice()->WaitForGPU();
        Check(geometry(bank)!=bankRest,"Player capsule left grass vertices unchanged");
        DespawnRuntimePlayer(scene,player);
        std::cout<<"GRASS_CURSOR_AND_PLAYER_GPU_VERTEX_RESPONSE_PASS"<<std::endl;

        render.scene=&cold.GetScene();
        for(int i=0;i<15;++i)frame();
        wi::graphics::GetDevice()->WaitForGPU();
        render.scene=&scene;
        Check(Capture(cold.GetScene())==both,"First native updates after cold reopen lost a painted layer");
        std::cout<<"GRASS_PALETTE_MIX_IMPORT_HISTORY_COLD_REOPEN_PASS"<<std::endl;
    }
}
