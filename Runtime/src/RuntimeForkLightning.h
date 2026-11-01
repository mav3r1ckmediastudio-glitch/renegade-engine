#pragma once
// G4-F1 isolated procedural fork-lightning visual review.
// Separate from accepted sheet-lightning shader, weather save data and combat.
#include "renegade/bridge/ForkLightningService.h"
#include "RuntimeSheetLightning.h"
#include "RuntimeThunderAudio.h"
#include "renegade/bridge/LightningAuthoringService.h"
#include <WickedEngine.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <string>

namespace renegade::runtime
{
    class RuntimeForkLightning
    {
    public:
        void Update(wi::scene::Scene& scene,std::uint64_t sceneRevision,
                    const XMFLOAT3& observer,const XMFLOAT3& forward,float dt)
        {
            bridge::LightningAuthoringState authored;
            if(scene.weathers.GetCount())
                authored=bridge::CaptureLightning(scene,scene.weathers.GetEntity(0));
            const bool active=authored.mode==bridge::LightningMode::Fork ||
                              authored.mode==bridge::LightningMode::Both;
            const bool preview=PreviewRequested();
            if(!active && !preview) {
                if(initialized_) { if(revision_==sceneRevision) Remove(scene); scheduler_.Reset(); forkSheetGlow_.Reset();
                    initialized_=false;revision_=0;lastEvent_=0; }
                if(authored.mode==bridge::LightningMode::Off)
                    RuntimeSheetLightning::ApplyFrame(scene,{});
                return;
            }
            const float frequency=preview && !active?12.0f:authored.forkPerMinute;
            const float density=preview && !active?1.0f:authored.forkBranchDensity;
            if(!initialized_ || revision_!=sceneRevision || frequency_!=frequency || branchDensity_!=density || glowIntensity_!=authored.sheetIntensity)
            {
                // Do not modify any saved weather or the G4 sheet-lightning
                // fields. New-scene entities are independent ephemeral VFX.
                // Previous scene owns previous bolt IDs; never remove those IDs
                // from a newly loaded scene where they might have been reused.
                if(initialized_ && revision_==sceneRevision) Remove(scene);
                objects_.fill(wi::ecs::INVALID_ENTITY);
                bridge::ForkLightningSettings opts;
                opts.enabled=true;
                opts.eventsPerMinute=frequency;
                opts.branchDensity=density;
                opts.cloudBaseMetres=scene.weather.volumetricCloudParameters.cloudStartHeight+
                    std::min(0.10f*scene.weather.volumetricCloudParameters.cloudThickness,400.0f);
                opts.seed=0xB017F0A1u;
                scheduler_.Configure(opts);
                bridge::SheetLightningSettings sheetOpts;
                sheetOpts.enabled = true;
                sheetOpts.eventsPerMinute = 10.0f;
                sheetOpts.intensity = active ? authored.sheetIntensity : 0.14f;
                sheetOpts.clusterSpreadMetres = 720.0f;
                sheetOpts.cloudLightRangeMetres = 2340.0f;
                forkSheetGlow_.Configure(sheetOpts);
                revision_=sceneRevision;frequency_=frequency;branchDensity_=density;glowIntensity_=authored.sheetIntensity;initialized_=true;lastEvent_=0;
            }

            if(!scene.weather.IsVolumetricClouds())
            {
                Remove(scene);
                forkSheetGlow_.Reset();
                RuntimeSheetLightning::ApplyFrame(scene,{});
                return;
            }
            scheduler_.Advance(dt,observer,forward);
            const auto& strike=scheduler_.Frame();
            if(strike.id && strike.id!=lastEvent_)
            {
                Remove(scene);
                lastEvent_=strike.id;
                // Trigger exactly the owner-approved sheet-lightning
                // formation at the fork origin, not a second glow algorithm.
                forkSheetGlow_.TriggerAtPosition(strike.origin,
                    scene.weather.volumetricCloudParameters.cloudThickness);
                objects_[0]=Create(scene,strike,observer,false);
                objects_[1]=Create(scene,strike,observer,true);
                ThunderAudio().Queue(strike.contact,observer,authored.thunder && active || preview,authored.thunderVolume,authored.distanceDelay,authored.thunderSoundSet);
                Trace("FORK_FLASH id="+std::to_string(strike.id)+
                      " segments="+std::to_string(strike.segments.size())+
                      " originY="+std::to_string(strike.origin.y)+
                      " distance="+std::to_string(std::hypot(
                          strike.contact.x-observer.x,
                          strike.contact.z-observer.z)));
            }
            // The bolt is the single timing source for both geometry and
            // cloud illumination. Do NOT advance an independent sheet
            // pulse timer: its later shimmer caused the extra post-bolt flash.
            const auto linkedGlow = forkSheetGlow_.FrameDrivenByStrike(
                strike.visible ? strike.brightness : 0.0f,
                strike.ageSeconds);
            if(authored.mode!=bridge::LightningMode::Both || strike.visible ||
               (strike.id && strike.ageSeconds<0.62f))
                RuntimeSheetLightning::ApplyFrame(scene, linkedGlow);
            // Preserve geometry through the brief dark gap between return
            // strokes; otherwise the second stroke would disappear entirely.
            // Remove both meshes after the tail has completed.
            if(strike.id && strike.ageSeconds>=0.42f)
                Remove(scene);
            else
            {
                for(unsigned i=0;i<objects_.size();++i)
                    if(auto* mat=scene.materials.GetComponent(objects_[i]))
                    {
                        const float value=strike.visible?strike.brightness:0.0f;
                        mat->baseColor.w=value*(i?0.22f:0.97f);
                        mat->emissiveColor.w=value*(i?3.0f:24.0f);
                        mat->SetDirty();
                    }
            }
        }

        void Reset(wi::scene::Scene& scene) noexcept
        {
            Remove(scene);RuntimeSheetLightning::ApplyFrame(scene,{});
            forkSheetGlow_.Reset();scheduler_.Reset();
            initialized_=false;revision_=0;lastEvent_=0;
        }

    private:
        static void Trace(const std::string& message)
        {
            if(const char* path=std::getenv("RENEGADE_G4_FORK_LIGHTNING_TRACE"))
            {
                std::ofstream file(path,std::ios::app);
                if(file) file<<message<<'\n';
            }
        }
        [[nodiscard]] static bool PreviewRequested() noexcept
        {
            static const bool on=[]()
            {
                const char* env=std::getenv("RENEGADE_G4_FORK_LIGHTNING_PREVIEW");
                return env && env[0]=='1' && env[1]=='\0';
            }();
            return on;
        }

        static XMFLOAT3 Offset(const XMFLOAT3& a,const XMVECTOR& v,float factor)
        {
            XMFLOAT3 out{};
            XMStoreFloat3(&out,XMLoadFloat3(&a)+v*factor);
            return out;
        }

        static wi::ecs::Entity Create(wi::scene::Scene& scene,
              const bridge::ForkLightningFrame& strike,
              const XMFLOAT3& observer,bool halo)
        {
            wi::profiler::ScopedRangeCPU createTiming("Fork Mesh Create");
            const auto id=wi::ecs::CreateEntity();
            scene.names.Create(id).name=halo?
                "G4 fork lightning faint diffused halo (preview)" :
                "G4 fork lightning tapered white channel (preview)";
            scene.transforms.Create(id).UpdateTransform();
            auto& object=scene.objects.Create(id);
            object.meshID=id;object.SetCastShadow(false);
            object.filterMask=wi::enums::FILTER_TRANSPARENT;
            auto& material=scene.materials.Create(id);
            material.shaderType=wi::scene::MaterialComponent::SHADERTYPE_UNLIT;
            material.baseColor=halo?XMFLOAT4{.28f,.48f,.92f,.10f}:
                XMFLOAT4{.90f,.95f,1.00f,.85f};
            material.emissiveColor=halo?XMFLOAT4{.34f,.56f,1.0f,1.0f}:
                XMFLOAT4{.95f,.97f,1.0f,14.0f};
            material.userBlendMode=wi::enums::BLENDMODE_ADDITIVE;
            material.SetDoubleSided(true);material.SetCastShadow(false);
            material.SetUseVertexColors(true); // independent per-segment root/branch fade
            material.SetDirty();

            auto& mesh=scene.meshes.Create(id);
            mesh.SetDoubleSided(true);
            for(const auto& line:strike.segments)
            {
                const XMVECTOR a=XMLoadFloat3(&line.from);
                const XMVECTOR b=XMLoadFloat3(&line.to);
                const XMVECTOR tangent=XMVector3Normalize(b-a);
                const XMVECTOR middle=(a+b)*0.5f;
                const XMVECTOR facing=XMVector3Normalize(
                    XMLoadFloat3(&observer)-middle);
                XMVECTOR right=XMVector3Cross(tangent,facing);
                if(XMVectorGetX(XMVector3LengthSq(right))<1e-5f)
                    right=XMVector3Cross(tangent,XMVectorSet(1,0,0,0));
                right=XMVector3Normalize(right);
                // Several alpha-weighted strips form a soft blue aura
                // around a narrow white-hot core. The electric leader has
                // energy 0 inside the cloud, ramping up while emerging.
                // No new shader or screen-space overlay.
                // One dominant white-hot channel, with thinner, dimmer branches.
                // Only the trunk gains ~25% thickness; branch widths stay put.
                const bool trunk=line.order==0;
                const float width=line.radiusMetres*
                    (halo?(trunk?16.9f:13.5f):(trunk?5.625f:4.5f));
                const float endWidth=width*(line.order==0?0.98f:0.90f);
                constexpr float sideways[5]={-1.0f,-0.34f,0.0f,0.34f,1.0f};
                const float weights[5]={
                    halo?0.0f:0.10f,
                    halo?0.30f:0.94f,
                    halo?0.72f:1.0f,
                    halo?0.30f:0.94f,
                    halo?0.0f:0.10f
                };
                const auto base=std::uint32_t(mesh.vertex_positions.size());
                XMFLOAT3 normal{};XMStoreFloat3(&normal,facing);
                for(unsigned side=0;side<5;++side)
                {
                    const float alpha=std::clamp(line.energy*weights[side],0.0f,1.0f);
                    const uint32_t color=wi::math::CompressColor(
                        XMFLOAT4(1.0f,1.0f,1.0f,alpha));
                    mesh.vertex_positions.push_back(Offset(line.from,right,sideways[side]*width));
                    mesh.vertex_positions.push_back(Offset(line.to,right,sideways[side]*endWidth));
                    mesh.vertex_normals.push_back(normal);
                    mesh.vertex_normals.push_back(normal);
                    mesh.vertex_colors.push_back(color);
                    mesh.vertex_colors.push_back(color);
                }
                for(unsigned side=0;side<4;++side)
                {
                    const uint32_t a=base+side*2;
                    mesh.indices.push_back(a);
                    mesh.indices.push_back(a+1);
                    mesh.indices.push_back(a+2);
                    mesh.indices.push_back(a+2);
                    mesh.indices.push_back(a+1);
                    mesh.indices.push_back(a+3);
                }
            }
            if(mesh.indices.empty()) {scene.Entity_Remove(id);return wi::ecs::INVALID_ENTITY;}
            auto& subset=mesh.subsets.emplace_back();
            subset.indexOffset=0;subset.indexCount=std::uint32_t(mesh.indices.size());
            subset.materialID=id;
            mesh.CreateRenderData();
            return id;
        }

        void Remove(wi::scene::Scene& scene) noexcept
        {
            for(auto& entity:objects_)
            {
                if(entity!=wi::ecs::INVALID_ENTITY)
                {
                    const auto* name=scene.names.GetComponent(entity);
                    if(name && name->name.find("G4 fork lightning ")==0)
                        scene.Entity_Remove(entity);
                    entity=wi::ecs::INVALID_ENTITY;
                }
            }
        }

        bridge::ForkLightningController scheduler_{};
        bridge::SheetLightningController forkSheetGlow_{};
        std::array<wi::ecs::Entity,2> objects_{
            wi::ecs::INVALID_ENTITY,wi::ecs::INVALID_ENTITY};
        std::uint64_t revision_=0,lastEvent_=0;
        float frequency_=-1.0f,branchDensity_=-1.0f,glowIntensity_=-1.0f;
        bool initialized_=false;
    };
}
