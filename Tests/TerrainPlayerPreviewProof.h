#pragma once
#include "renegade/bridge/PlayerCameraPreviewService.h"
#include <map>
#include <memory>
#include <stdexcept>

namespace terrain_player_preview_proof
{
    using BindingKey=std::pair<wi::ecs::Entity,std::size_t>;
    using Bindings=std::map<BindingKey,int>;

    inline Bindings CaptureBindings(wi::scene::Scene& scene)
    {
        Bindings result;
        auto* gpu=wi::graphics::GetDevice();
        for(std::size_t i=0;i<scene.terrains.GetCount();++i)
            for(const auto& entry:scene.terrains[i].chunks)
            {
                const auto* material=scene.materials.GetComponent(entry.second.entity);
                if(!material) continue;
                for(std::size_t slot=0;slot<4;++slot)
                {
                    const auto& resource=material->textures[slot].resource;
                    if(resource.IsValid())
                        result[{entry.second.entity,slot}]=gpu->GetDescriptorIndex(
                            &resource.GetTexture(),wi::graphics::SubresourceType::SRV);
                }
            }
        return result;
    }

    inline auto CaptureEncoded(wi::scene::Scene& scene)
    {
        std::map<std::pair<int,int>,std::vector<std::uint16_t>> result;
        for(const auto& entry:scene.terrains[0].chunks)
            result[{entry.first.x,entry.first.z}]=entry.second.heightmap_data;
        return result;
    }

    inline void Run(wi::scene::Scene& scene,wi::RenderPath3D& main,
                    const std::filesystem::path& out, bool hostedOffscreen, int previewCheckpoint)
    {
        using namespace renegade::bridge;
        auto* gpu=wi::graphics::GetDevice();
        std::size_t mainDraws=0,previewDraws=0;
        const auto frame=[&](PlayerCameraPreviewService* preview, bool drawFrame)
        {
            wi::eventhandler::FireEvent(wi::eventhandler::EVENT_THREAD_SAFE_POINT,0);
            main.PreUpdate();
            main.Update(1.f/60.f);
            const bool updatePreview=preview&&preview->NeedsRender();
            // Match Studio: main Scene update, preview update; preview
            // PreRender/Render precedes the main terrain's draw.
            if(updatePreview)
            {
                preview->PreUpdate();
                preview->Update(0);
                if(drawFrame) preview->PreRender();
            }
            const bool drawMain=drawFrame && (!hostedOffscreen || preview==nullptr);
            if(drawMain) main.PreRender();
            if(drawFrame && updatePreview) { preview->Render(); ++previewDraws; }
            if(drawMain) { main.Render(); ++mainDraws; }
            gpu->SubmitCommandLists();
            wi::renderer::UpdateGPUSuballocator();
        };
        // Hosted cases divide expensive rasterization between source captures
        // (-1) and each preview (0..2). Every case keeps all three native rebuilds,
        // twelve updates apiece and every binding/sculpt/encoded-height assertion.
        // -2 retains every rebuild/update/binding/height assertion without a
        // duplicate draw; another maximum-terrain case supplies that checkpoint.
        const bool drawSource=!hostedOffscreen || previewCheckpoint==-1;
        std::cout<<"PLAYER_PREVIEW_CHECKPOINT="<<previewCheckpoint<<std::endl;
        if(drawSource) std::cout<<"PLAYER_PREVIEW_STAGE source-before render"<<std::endl;
        frame(nullptr,drawSource);
        gpu->WaitForGPU();
        const auto before=CaptureBindings(scene);
        if(before.size()!=scene.terrains[0].chunks.size()*4)
            throw std::runtime_error("Player preview proof requires all native terrain atlas bindings");
        const auto sculptBefore=CaptureTerrainSculpt(scene,scene.terrains[0]);
        const auto encodedBefore=CaptureEncoded(scene);
        const auto unchanged=[&]()
        {
            if(CaptureBindings(scene)!=before)
                throw std::runtime_error("Player Start preview rebound the editor terrain atlas");
            auto expected=sculptBefore,actual=CaptureTerrainSculpt(scene,scene.terrains[0]);
            if(RetainChangedTerrainSculpt(expected,actual)!=0)
                throw std::runtime_error("Player Start preview changed authored terrain");
        };
        gpu->WaitForGPU();
        if(drawSource && !wi::helper::saveTextureToFile(main.GetRenderResult3D(),
            (out/"terrain-before-player-preview.png").generic_u8string()))
            throw std::runtime_error("Player preview before capture failed");
        for(int cycle=0;cycle<3;++cycle)
        {
            std::cout<<"PLAYER_PREVIEW_STAGE rebuild="<<cycle<<" prepare"<<std::endl;
            PlayerStart start;
            start.transform.translation={float(cycle*70),0,float(cycle*90)};
            auto preview=std::make_unique<PlayerCameraPreviewService>();
            std::string error;
            if(!preview->Prepare(scene,start,"","",error))
                throw std::runtime_error("Player preview preparation failed: "+error);
            unchanged();
            if(CaptureEncoded(*preview->scene)!=encodedBefore)
                throw std::runtime_error("Player preview clone lost authored terrain heights");
            for(int i=0;i<12;++i)
            {
                // Keep all twelve native updates and every binding/height check.
                // Separate hosted cases cover each actual preview render.
                // Desktop retains every paired Studio presentation frame.
                const bool drawFrame=!hostedOffscreen ||
                    (cycle==previewCheckpoint && i==11);
                if(drawFrame) std::cout<<"PLAYER_PREVIEW_STAGE rebuild="<<cycle
                    <<" draw-update="<<i<<std::endl;
                frame(preview.get(),drawFrame);
                unchanged();
                if(CaptureEncoded(*preview->scene)!=encodedBefore)
                    throw std::runtime_error("Player preview regenerated the sculpted landscape");
            }
            gpu->WaitForGPU();
            unchanged();
            if(hostedOffscreen && cycle==previewCheckpoint &&
                !wi::helper::saveTextureToFile(preview->GetRenderResult3D(),
                    (out/("terrain-player-preview-rebuild-"+std::to_string(cycle)+".png")).generic_u8string()))
                throw std::runtime_error("Hosted preview capture failed");
            preview.reset();
            for(int i=0;i<3;++i) frame(nullptr,!hostedOffscreen);
            gpu->WaitForGPU();
            unchanged();
        }
        if(drawSource) std::cout<<"PLAYER_PREVIEW_STAGE source-after render"<<std::endl;
        frame(nullptr,drawSource);
        gpu->WaitForGPU();
        unchanged();
        const std::size_t expectedMain=previewCheckpoint==-1?2:0;
        const std::size_t expectedPreview=previewCheckpoint<0?0:1;
        if(hostedOffscreen && (mainDraws!=expectedMain || previewDraws!=expectedPreview))
            throw std::runtime_error("Hosted preview proof lost a required GPU checkpoint");
        if(drawSource && !wi::helper::saveTextureToFile(main.GetRenderResult3D(),
            (out/"terrain-after-player-preview.png").generic_u8string()))
            throw std::runtime_error("Player preview after capture failed");
        std::cout<<"PLAYER_PREVIEW_PRIVATE_TERRAIN_ATLAS_PASS bindings="
            <<before.size()<<" rebuilds=3 authored_terrain_unchanged=1"
            <<" main_draws="<<mainDraws<<" preview_draws="<<previewDraws
            <<" checkpoint="<<previewCheckpoint<<std::endl;
    }
}
