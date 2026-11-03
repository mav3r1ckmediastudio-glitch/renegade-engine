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
                    const std::filesystem::path& out)
    {
        using namespace renegade::bridge;
        auto* gpu=wi::graphics::GetDevice();
        const auto before=CaptureBindings(scene);
        if(before.size()!=scene.terrains[0].chunks.size()*4)
            throw std::runtime_error("Player preview proof requires all native terrain atlas bindings");
        const auto sculptBefore=CaptureTerrainSculpt(scene,scene.terrains[0]);
        const auto encodedBefore=CaptureEncoded(scene);
        const auto frame=[&](PlayerCameraPreviewService* preview)
        {
            wi::eventhandler::FireEvent(wi::eventhandler::EVENT_THREAD_SAFE_POINT,0);
            main.PreUpdate();
            main.Update(1.f/60.f);
            const bool draw=preview&&preview->NeedsRender();
            // Match Studio: main Scene update, preview update; preview
            // PreRender/Render precedes the main terrain's draw.
            if(draw)
            {
                preview->PreUpdate();
                preview->Update(0);
                preview->PreRender();
            }
            main.PreRender();
            if(draw) preview->Render();
            main.Render();
            gpu->SubmitCommandLists();
            wi::renderer::UpdateGPUSuballocator();
        };
        const auto unchanged=[&]()
        {
            if(CaptureBindings(scene)!=before)
                throw std::runtime_error("Player Start preview rebound the editor terrain atlas");
            auto expected=sculptBefore,actual=CaptureTerrainSculpt(scene,scene.terrains[0]);
            if(RetainChangedTerrainSculpt(expected,actual)!=0)
                throw std::runtime_error("Player Start preview changed authored terrain");
        };
        gpu->WaitForGPU();
        if(!wi::helper::saveTextureToFile(main.GetRenderResult3D(),
            (out/"terrain-before-player-preview.png").generic_u8string()))
            throw std::runtime_error("Player preview before capture failed");
        for(int cycle=0;cycle<3;++cycle)
        {
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
                frame(preview.get());
                unchanged();
                if(CaptureEncoded(*preview->scene)!=encodedBefore)
                    throw std::runtime_error("Player preview regenerated the sculpted landscape");
            }
            gpu->WaitForGPU();
            preview.reset();
            for(int i=0;i<3;++i) frame(nullptr);
            gpu->WaitForGPU();
            unchanged();
        }
        if(!wi::helper::saveTextureToFile(main.GetRenderResult3D(),
            (out/"terrain-after-player-preview.png").generic_u8string()))
            throw std::runtime_error("Player preview after capture failed");
        std::cout<<"PLAYER_PREVIEW_PRIVATE_TERRAIN_ATLAS_PASS bindings="
            <<before.size()<<" rebuilds=3 authored_terrain_unchanged=1"<<std::endl;
    }
}
