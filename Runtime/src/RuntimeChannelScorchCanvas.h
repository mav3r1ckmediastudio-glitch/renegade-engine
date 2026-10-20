#pragma once
#include "RuntimeImpactMarks.h"
#include "RuntimeProjectileWorld.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace renegade::runtime {
// Persistent beam painting is accumulated into area-based native decals.
// Decal count scales with painted wall area, not frames or repeated brush dabs.
// GPU texture uploads are batched once per dirty tile per simulation frame.
struct RuntimeChannelScorchCanvas {
    static constexpr unsigned Resolution=192;
    static constexpr float PatchSize=3.f;
    static constexpr float BrushRadius=.18f;
    static constexpr float Lifetime=120.f;
    static constexpr size_t MaxPatches=96;
    struct Patch {
        wi::ecs::Entity decal=wi::ecs::INVALID_ENTITY;
        wi::ecs::Entity receiver=wi::ecs::INVALID_ENTITY;
        RuntimeImpactMarks::Anchor anchor;
        XMFLOAT3 normal{};
        wi::graphics::Texture texture;
        std::vector<std::uint8_t> pixels;
        float remaining=Lifetime;
        bool dirty=true;
    };
    std::vector<Patch> patches;
    std::uint64_t paintedDabs=0;
    void Reset(wi::scene::Scene& scene) {
        for(const auto& p:patches)scene.Entity_Remove(p.decal);
        patches.clear();paintedDabs=0;
    }
    static bool Project(const wi::scene::Scene& scene,const Patch& patch,const XMFLOAT3& point,
                        XMFLOAT3& local) {
        const auto* transform=scene.transforms.GetComponent(patch.decal);
        if(!transform)return false;
        const auto matrix=transform->GetWorldMatrix();
        const auto inverse=XMMatrixInverse(nullptr,matrix);
        XMStoreFloat3(&local,XMVector3TransformCoord(XMLoadFloat3(&point),inverse));
        return std::isfinite(local.x)&&std::isfinite(local.y)&&std::isfinite(local.z);
    }
    // A sample must fit wholly inside its patch, preventing holes at seams.
    // Wicked's box_to_uv() maps decal-local [-1,+1] into [0,1].
    // Decal geometry is a unit half-width cube, not a half-metre UV tile.
    // These conversions must match shadingHF.hlsli and globals.hlsli.
    static XMFLOAT2 UV(const XMFLOAT3& local) noexcept {
        return {local.x*.5f+.5f, .5f-local.y*.5f};
    }
    static bool Fits(const XMFLOAT3& local) {
        constexpr float border=BrushRadius/PatchSize+.025f;
        return std::abs(local.x)<=1.f-border &&
               std::abs(local.y)<=1.f-border && std::abs(local.z)<.6f;
    }
    Patch* Create(wi::scene::Scene& scene,const bridge::ProjectileImpact& impact,
                  wi::ecs::Entity receiver) {
        auto* device=wi::graphics::GetDevice();
        if(!device || patches.size()>=MaxPatches)return nullptr;
        const auto point=ProjectileNativeVector(impact.contact.position);
        const auto normal=ProjectileNativeVector(impact.contact.normal);
        auto n=XMLoadFloat3(&normal);
        if(XMVectorGetX(XMVector3LengthSq(n))<.00001f)return nullptr;
        n=XMVector3Normalize(n);
        XMFLOAT3 outward;XMStoreFloat3(&outward,n);
        RuntimeImpactMarks::Anchor anchor;
        if(!RuntimeImpactMarks::Capture(scene,point,outward,receiver,anchor))return nullptr;
        anchor.size=PatchSize;
        anchor.roll=0;
        const auto entity=scene.Entity_CreateDecal("Channel painted soot patch","","");
        auto* material=scene.materials.GetComponent(entity);
        if(!material){scene.Entity_Remove(entity);return nullptr;}
        material->baseColor={1,1,1,1};
        material->userBlendMode=wi::enums::BLENDMODE_ALPHA;
        material->SetRoughness(1);material->SetReflectance(.01f);
        material->emissiveColor={0,0,0,0};material->SetDirty();
        if(!RuntimeImpactMarks::Follow(scene,entity,anchor)){
            scene.Entity_Remove(entity);return nullptr;
        }
        patches.emplace_back();
        auto& result=patches.back();
        result.decal=entity;result.receiver=receiver;result.normal=outward;
        result.anchor=anchor;result.pixels.resize(Resolution*Resolution*4);
        return &result;
    }
    static void Dab(Patch& patch,const XMFLOAT3& local) {
        const auto uv=UV(local);
        const float cx=uv.x*Resolution,cy=uv.y*Resolution;
        const float radius=BrushRadius/(2.f*PatchSize)*Resolution;
        const int minX=std::max(0,int(std::floor(cx-radius-2)));
        const int maxX=std::min(int(Resolution)-1,int(std::ceil(cx+radius+2)));
        const int minY=std::max(0,int(std::floor(cy-radius-2)));
        const int maxY=std::min(int(Resolution)-1,int(std::ceil(cy+radius+2)));
        for(int y=minY;y<=maxY;++y)for(int x=minX;x<=maxX;++x) {
            const float dx=(float(x)+.5f-cx)/radius;
            const float dy=(float(y)+.5f-cy)/radius;
            const float dist=std::sqrt(dx*dx+dy*dy);
            const float noise=RuntimeImpactMarks::SootNoise(float(x)*.21f,float(y)*.21f);
            const float edge=.78f+.12f*noise+.08f*
                RuntimeImpactMarks::SootNoise(float(x)*.63f,float(y)*.63f);
            const float body=std::clamp((edge-dist)*8.f,0.f,1.f);
            const float halo=std::clamp((edge+.18f-dist)*5.f,0.f,1.f)*.19f;
            const auto alpha=std::uint8_t(std::clamp(
                (body*(.6f+.35f*noise)+halo)*255.f,0.f,255.f));
            if(!alpha)continue;
            const unsigned pixel=(unsigned(y)*Resolution+unsigned(x))*4;
            // Alpha accumulates without creating an over-dark overlapping
            // stack of independently projected translucent decals.
            patch.pixels[pixel+0]=12;
            patch.pixels[pixel+1]=10;
            patch.pixels[pixel+2]=9;
            patch.pixels[pixel+3]=std::max(patch.pixels[pixel+3],alpha);
        }
        patch.dirty=true;patch.remaining=Lifetime;
    }
    // Return false only if there is no valid receiver/patch or paint budget.
    bool Paint(wi::scene::Scene& scene,const bridge::ProjectileImpact& impact,
               wi::ecs::Entity receiver) {
        if(receiver==wi::ecs::INVALID_ENTITY ||
           impact.contact.surfaceType==bridge::ImpactSurfaceType::Character ||
           impact.contact.surfaceType==bridge::ImpactSurfaceType::Water ||
           !bridge::FiniteProjectileVector(impact.contact.position) ||
           !bridge::FiniteProjectileVector(impact.contact.normal))return false;
        const auto point=ProjectileNativeVector(impact.contact.position);
        const auto contactNormal=ProjectileNativeVector(impact.contact.normal);
        const auto n=XMVector3Normalize(XMLoadFloat3(&contactNormal));
        Patch* selected=nullptr;XMFLOAT3 local{};
        for(auto& patch:patches) {
            if(patch.receiver!=receiver)continue;
            if(XMVectorGetX(XMVector3Dot(n,XMLoadFloat3(&patch.normal)))<.96f)continue;
            XMFLOAT3 candidate;
            if(!Project(scene,patch,point,candidate)||!Fits(candidate))continue;
            selected=&patch;local=candidate;break;
        }
        if(!selected) {
            selected=Create(scene,impact,receiver);
            if(!selected || !Project(scene,*selected,point,local))return false;
        }
        Dab(*selected,local);++paintedDabs;return true;
    }
    bool UploadDirty(wi::scene::Scene& scene) {
        auto* device=wi::graphics::GetDevice();
        if(!device)return false;
        bool any=false;
        for(auto& patch:patches) {
            if(!patch.dirty)continue;
            wi::graphics::TextureDesc desc;
            desc.width=desc.height=Resolution;
            desc.format=wi::graphics::Format::R8G8B8A8_UNORM;
            desc.bind_flags=wi::graphics::BindFlag::SHADER_RESOURCE;
            wi::graphics::SubresourceData data;
            data.data_ptr=patch.pixels.data();data.row_pitch=Resolution*4;
            data.slice_pitch=patch.pixels.size();
            wi::graphics::Texture next;
            if(!device->CreateTexture(&desc,&data,&next))continue;
            auto* material=scene.materials.GetComponent(patch.decal);
            if(!material)continue;
            patch.texture=std::move(next);
            material->textures[wi::scene::MaterialComponent::BASECOLORMAP].resource.SetTexture(patch.texture);
            material->SetDirty();
            patch.dirty=false;any=true;
        }
        return any;
    }
    void Sync(wi::scene::Scene& scene,float dt) {
        if(dt>0 && std::isfinite(dt))for(auto it=patches.begin();it!=patches.end();) {
            it->remaining-=dt;
            if(it->remaining<=0 || !scene.decals.Contains(it->decal) ||
               !scene.transforms.Contains(it->receiver)) {
                scene.Entity_Remove(it->decal);
                it=patches.erase(it);
            } else ++it;
        }
        UploadDirty(scene);
    }
    bool Refresh(wi::scene::Scene& scene) {
        bool changed=false;
        for(auto& patch:patches)
            if(RuntimeImpactMarks::Follow(scene,patch.decal,patch.anchor))changed=true;
        return changed;
    }
};
}
