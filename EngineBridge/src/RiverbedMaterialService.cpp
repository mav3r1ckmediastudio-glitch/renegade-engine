#include "renegade/bridge/RiverbedMaterialService.h"
#include "renegade/bridge/RiverTerrainLayerService.h"
#include "renegade/bridge/RiverSplineService.h"
#include "renegade/bridge/SpecialistComponentService.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <limits>
#include <iterator>
#include <stdexcept>

namespace renegade::bridge
{
    namespace
    {
        [[nodiscard]] bool ValidRockTerrain(
            const wi::scene::Scene& scene,const wi::terrain::Terrain& t)
        {
            if(t.IsCenterToCamEnabled() || t.IsRemovalEnabled() ||
               t.chunks.empty() || t.chunks.size()>81 ||
               t.materialEntities.size()<4 || t.materialEntities.size()>5)return false;
            if(t.materialEntities.size()==5)
            {
                const auto id=t.materialEntities.back();
                const auto* name=scene.names.GetComponent(id);
                if(!name || name->name!="Renegade Rocky Riverbed" ||
                    !scene.materials.Contains(id))return false;
            }
            for(const auto& entry:t.chunks)
            {
                const auto& c=entry.second;
                if(c.invalidated || c.blendmap_layers.size()!=t.materialEntities.size())return false;
                const auto* mesh=scene.meshes.GetComponent(c.entity);
                if(!mesh || mesh->vertex_positions.size()!=wi::terrain::vertexCount)
                    return false;
                for(const auto& layer:c.blendmap_layers)
                    if(layer.pixels.size()!=wi::terrain::vertexCount)return false;
            }
            return true;
        }
        void RefreshNativeTerrainBlendmap(wi::terrain::Terrain& terrain,
            wi::terrain::ChunkData& chunk)
        {
            // CreateChunkRegionTexture uploads only when array shape changes.
            // Shared-layer edits retain five slots: rebuild its GPU texture too.
            // Caller has already drained GPU work; heights remain untouched.
            chunk.blendmap = {};
            terrain.CreateChunkRegionTexture(chunk);
            if(chunk.vt)chunk.vt->invalidate();
        }
    }

    bool RebindBundledRockyRiverbedMaterials(
        wi::scene::Scene& scene,std::string& error)
    {
        error.clear();
        // SceneService may open in headless authoring tools. Never dereference
        // resourcemanager's graphics path without a native GPU.
        if(wi::graphics::GetDevice()==nullptr)return true;
        const auto executable=std::filesystem::u8path(
            wi::helper::GetExecutablePath());
        const auto assets=executable.parent_path()/"Content"/"terrain"/
            "riverbed"/"rocky_rugged";
        constexpr const char* name="Renegade Rocky Riverbed";
        const struct Map
        {
            wi::scene::MaterialComponent::TEXTURESLOT slot;
            const char* filename;
        } maps[]={
            {wi::scene::MaterialComponent::BASECOLORMAP,"rocky_basecolor.png"},
            {wi::scene::MaterialComponent::NORMALMAP,"rocky_normal_dx.png"},
            {wi::scene::MaterialComponent::SURFACEMAP,"rocky_surface.png"}
        };
        for(std::size_t terrainIndex=0;
            terrainIndex<scene.terrains.GetCount();++terrainIndex)
        {
            auto& terrain=scene.terrains[terrainIndex];
            for(const auto entity:terrain.materialEntities)
            {
                const auto* label=scene.names.GetComponent(entity);
                if(!label || label->name!=name)continue;
                auto* material=scene.materials.GetComponent(entity);
                if(!material)
                {
                    error="Saved Rocky Riverbed is missing its material component";
                    return false;
                }
                // Only repair the original V1 authored layout; do not
                // overwrite arbitrary user custom materials or paths.
                for(const auto& m:maps)
                {
                    const auto& file=material->textures[m.slot].name;
                    if(!file.empty() &&
                       std::filesystem::u8path(file).filename()!=m.filename)
                    {
                        error="Rocky Riverbed texture layout differs from the "
                            "original supplied PBR maps; no automatic rebind";
                        return false;
                    }
                }
                std::vector<wi::Resource> hydrated;
                hydrated.reserve(3);
                for(const auto& map:maps)
                {
                    const auto path=assets/map.filename;
                    std::error_code io;
                    if(!std::filesystem::is_regular_file(path,io) || io)
                    {
                        error="Missing bundled Rocky Riverbed map: "+
                            path.generic_u8string();
                        return false;
                    }
                    wi::Resource resource=wi::resourcemanager::Load(
                        path.generic_u8string());
                    if(!resource.IsValid() || !resource.GetTexture().IsValid())
                    {
                        error="Wicked failed to load Rocky Riverbed map: "+
                            path.generic_u8string();
                        return false;
                    }
                    hydrated.push_back(std::move(resource));
                }
                for(std::size_t i=0;i<std::size(maps);++i)
                {
                    auto& slot=material->textures[maps[i].slot];
                    slot.name=(assets/maps[i].filename).generic_u8string();
                    slot.resource=std::move(hydrated[i]);
                }
                // Upgrade the experimental first-run 0.25x boulder scale.
                // The V1 prototype had no user-adjustable scale setting.
                if(std::abs(material->texMulAdd.x-.25f)<.0001f &&
                   std::abs(material->texMulAdd.y-.25f)<.0001f)
                {
                    material->texMulAdd.x=1.f;
                    material->texMulAdd.y=1.f;
                }
                material->CreateRenderData();
                // Dirty source materials trigger procedural Generation_Restart,
                // which destroys the authored sculpt on scene adoption. GPU
                // hydration only needs render data plus virtual tile invalidation.
                material->SetDirty(false);
                // The material is baked into native terrain virtual textures:
                // force the current mask region to resample its surface.
                for(auto& chunk:terrain.chunks)
                    if(chunk.second.vt)chunk.second.vt->invalidate();
            }
        }
        return true;
    }

    PaintRockyRiverbedCommand::PaintRockyRiverbedCommand(
        wi::scene::Scene& scene,wi::ecs::Entity terrain,
        wi::ecs::Entity river,std::string assetDirectory)
        : scene_(&scene),terrain_(terrain),river_(river),
          assets_(std::move(assetDirectory)) {}

    bool PaintRockyRiverbedCommand::Prepare()
    {
        auto* terrain=scene_->terrains.GetComponent(terrain_);
        const auto* spline=scene_->splines.GetComponent(river_);
        const auto* water=scene_->materials.GetComponent(river_);
        if(!terrain || terrain->scene!=scene_ ||
           terrain->terrainEntity!=terrain_ || !spline || !water ||
           water->shaderType!=wi::scene::MaterialComponent::SHADERTYPE_WATER ||
           spline->terrain_modifier_amount!=0 ||
           !std::isfinite(spline->width) ||
           spline->width<.25f || spline->width>25.f)
        {
            error_="Select a supported native WATER spline on resident terrain";
            return false;
        }
        // V1 material is attached only to an already-authorised linked bed.
        // Do not automatically paint random water on unrelated terrain.
        RiverTerrainLayerSettings settings;
        if(!ReadRiverTerrainLayerSettings(*scene_,terrain_,river_,
                settings,error_) || !settings.linked)
        {
            if(error_.empty())error_="Link this river to its sculpted bed first";
            return false;
        }
        terrain->Generation_Cancel();
        if(!ValidRockTerrain(*scene_,*terrain))
        {
            error_="Rocky bed requires four original slots, optionally followed by its shared Rocky Riverbed slot";
            return false;
        }
        reuseMaterial_=terrain->materialEntities.size()==5;
        if(reuseMaterial_)materialEntity_=terrain->materialEntities.back();
        const auto points=SampleSplineWorldPoints(*scene_,river_,128);
        if(points.size()<2)
        {
            error_="Unable to sample river in world space";
            return false;
        }
        masks_.clear();
        std::size_t painted=0;
        for(auto& kv:terrain->chunks)
        {
            auto& c=kv.second;
            const auto* mesh=scene_->meshes.GetComponent(c.entity);
            const auto world=scene_->ComputeEntityMatrixRecursive(c.entity);
            ChunkMask mask;
            mask.entity=c.entity;
            mask.pixels.resize(wi::terrain::vertexCount,0);
            if(reuseMaterial_)mask.before.assign(c.blendmap_layers.back().pixels.begin(),
                c.blendmap_layers.back().pixels.end());
            for(std::size_t i=0;i<mask.pixels.size();++i)
            {
                XMFLOAT3 p;
                DirectX::XMStoreFloat3(&p,DirectX::XMVector3TransformCoord(
                    DirectX::XMLoadFloat3(&mesh->vertex_positions[i]),world));
                float minSq=std::numeric_limits<float>::max();
                for(std::size_t j=1;j<points.size();++j)
                {
                    const auto& a=points[j-1];
                    const auto& b=points[j];
                    const float dx=b.x-a.x,dz=b.z-a.z,len=dx*dx+dz*dz;
                    if(len<1e-6f)continue;
                    const float t=std::clamp(
                        ((p.x-a.x)*dx+(p.z-a.z)*dz)/len,0.f,1.f);
                    const float x=p.x-a.x-t*dx,z=p.z-a.z-t*dz;
                    minSq=std::min(minSq,x*x+z*z);
                }
                const float distance=std::sqrt(minSq);
                // Keep a rocky channel core, then blend over several terrain
                // vertices. A narrow 1m-grid feather looked like a hard ribbon.
                const float inner=std::max(.1f,.45f*spline->width);
                const float outer=spline->width+2.8f;
                // Continuous world-space variation breaks the parallel painted
                // edge without per-chunk seams or random changes on reapply.
                const float edgeVariation=.24f*std::sin(p.x*.71f+p.z*.43f)+
                    .16f*std::sin(p.x*1.37f-p.z*.91f);
                float alpha=std::clamp(
                    (outer-distance+edgeVariation)/(outer-inner),0.f,1.f);
                alpha=alpha*alpha*alpha*(alpha*(alpha*6.f-15.f)+10.f);
                mask.pixels[i]=std::uint8_t(std::lround(alpha*255.f));
                if(mask.pixels[i])++painted;
                if(reuseMaterial_)mask.pixels[i]=std::max(mask.pixels[i],mask.before[i]);
            }
            masks_.push_back(std::move(mask));
        }
        if(painted<10)
        {
            error_="River mask missed all terrain chunks; no changes made";
            masks_.clear();return false;
        }
        prepared_=true;
        return true;
    }

    bool PaintRockyRiverbedCommand::Apply()
    {
        auto* terrain=scene_->terrains.GetComponent(terrain_);
        if(!terrain || !ValidRockTerrain(*scene_,*terrain) ||
           masks_.size()!=terrain->chunks.size() ||
           terrain->materialEntities.size()!=(reuseMaterial_?5u:4u) ||
           (reuseMaterial_ && terrain->materialEntities.back()!=materialEntity_))
        {
            error_="Terrain material layout changed; refusing rocky mask application";
            return false;
        }
        auto* gpu=wi::graphics::GetDevice();
        if(!gpu)
        {
            error_="Rocky material application needs a native graphics device";
            return false;
        }
        // Pre-load all three maps before mutating ANY terrain/material arrays.
        wi::scene::MaterialComponent rocky;
        rocky.SetBaseColor({1,1,1,1});
        rocky.SetRoughness(1);
        rocky.SetMetalness(1);
        rocky.SetReflectance(.02f);
        rocky.SetNormalMapStrength(1.25f);
        rocky.SetOcclusionEnabled_Primary(true);
        // Native terrain VT ignores texMulAdd; repeat scale is supplied by
        // packed PBR maps. Reusing the shared layer preserves its material.
        rocky.texMulAdd={1.0f,1.0f,0,0};
        const std::filesystem::path base=std::filesystem::u8path(assets_);
        auto load=[&](int slot,const char* file)
        {
            const auto path=(base/file).lexically_normal();
            auto& tex=rocky.textures[slot];
            tex.name=path.generic_u8string();
            tex.resource=wi::resourcemanager::Load(tex.name);
            return tex.resource.IsValid() &&
                tex.resource.GetTexture().IsValid();
        };
        if(!reuseMaterial_ && (!load(wi::scene::MaterialComponent::BASECOLORMAP,
                "rocky_basecolor.png") ||
           !load(wi::scene::MaterialComponent::NORMALMAP,
                "rocky_normal_dx.png") ||
           !load(wi::scene::MaterialComponent::SURFACEMAP,
                "rocky_surface.png")))
        {
            error_="One or more rocky bed textures could not load; terrain unchanged";
            return false;
        }
        // Complete topology checks BEFORE adding the fifth slot.
        for(const auto& m:masks_)
        {
            bool found=false;
            for(const auto& kv:terrain->chunks)
                if(kv.second.entity==m.entity &&
                   m.pixels.size()==wi::terrain::vertexCount)
                    {
                        if(reuseMaterial_ && !std::equal(m.before.begin(),m.before.end(),
                            kv.second.blendmap_layers.back().pixels.begin()))
                        {error_="Rocky paint changed since this command was prepared";return false;}
                        found=true;break;
                    }
            if(!found){error_="Terrain chunks changed since river mask preparation";return false;}
        }
        terrain->Generation_Cancel();
        gpu->WaitForGPU();
        if(!reuseMaterial_)
        {
        if(materialEntity_==wi::ecs::INVALID_ENTITY)
            materialEntity_=wi::ecs::CreateEntity();
        if(scene_->materials.Contains(materialEntity_))
        {
            error_="Rocky material entity unexpectedly already exists";
            return false;
        }
        auto& assigned=scene_->materials.Create(materialEntity_);
        assigned=std::move(rocky);
        assigned.CreateRenderData();
        assigned.SetDirty(false);
        scene_->names.Create(materialEntity_)="Renegade Rocky Riverbed";
        terrain->materialEntities.push_back(materialEntity_);
        }
        for(auto& kv:terrain->chunks)
        {
            const auto it=std::find_if(masks_.begin(),masks_.end(),
                [&](const ChunkMask& m){return m.entity==kv.second.entity;});
            if(!reuseMaterial_)kv.second.blendmap_layers.emplace_back();
            auto& pixels=kv.second.blendmap_layers.back().pixels;
            pixels.assign(it->pixels.begin(),it->pixels.end());
            RefreshNativeTerrainBlendmap(*terrain,kv.second);
        }
        applied_=true;
        error_.clear();
        return true;
    }

    bool PaintRockyRiverbedCommand::Execute()
    {
        if(applied_){error_="Rocky material is already applied";return false;}
        if(!prepared_ && !Prepare())return false;
        return Apply();
    }
    void PaintRockyRiverbedCommand::Undo()
    {
        if(!applied_)return;
        auto* terrain=scene_->terrains.GetComponent(terrain_);
        if(!terrain || terrain->materialEntities.size()!=5 ||
           terrain->materialEntities.back()!=materialEntity_)return;
        for(const auto& kv:terrain->chunks)
        {
            if(kv.second.blendmap_layers.size()!=5)return;
            const auto it=std::find_if(masks_.begin(),masks_.end(),
                [&](const ChunkMask& m){return m.entity==kv.second.entity;});
            if(it==masks_.end() || !std::equal(it->pixels.begin(),it->pixels.end(),
                kv.second.blendmap_layers.back().pixels.begin()))return;
        }
        terrain->Generation_Cancel();
        if(auto* gpu=wi::graphics::GetDevice())gpu->WaitForGPU();
        for(auto& kv:terrain->chunks)
        {
            if(reuseMaterial_)
            {
                const auto it=std::find_if(masks_.begin(),masks_.end(),
                    [&](const ChunkMask& m){return m.entity==kv.second.entity;});
                kv.second.blendmap_layers.back().pixels.assign(
                    it->before.begin(),it->before.end());
            }
            else kv.second.blendmap_layers.pop_back();
            RefreshNativeTerrainBlendmap(*terrain,kv.second);
        }
        if(!reuseMaterial_)
        {
            terrain->materialEntities.pop_back();
            scene_->Entity_Remove(materialEntity_);
        }
        applied_=false;
    }
}
