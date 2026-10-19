#pragma once
// Grenade-only visual presentation: flame front -> debris -> turbulent soot/dust.
// All gameplay blast, damage, ownership and timing stay in ProjectileSession.
#include "renegade/bridge/ProjectileEffectRuntime.h"
#include <WickedEngine.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>
namespace renegade::runtime {
struct RuntimeGrenadeExplosionFX {
    enum Layer : unsigned { Fire=0, Shock=1, Soot=2, Dust=3, Embers=4, Count=5 };
    struct Burst { wi::ecs::Entity entity=wi::ecs::INVALID_ENTITY;float duration=0; };
    static float Noise(unsigned x,unsigned y,unsigned seed) {
        std::uint32_t h=x*0x9E3779B1u ^ y*0x85EBCA77u ^ (seed+1u)*0xC2B2AE3Du;
        h^=h>>16;h*=0x7FEB352Du;h^=h>>15;h*=0x846CA68Bu;h^=h>>16;
        return float(h & 65535u)/65535.f;
    }
    static float SmoothNoise(float x,float y,unsigned seed) {
        const int ix=int(std::floor(x)),iy=int(std::floor(y));
        float fx=x-float(ix),fy=y-float(iy);
        fx=fx*fx*(3.f-2.f*fx);fy=fy*fy*(3.f-2.f*fy);
        const auto n=[&](int xx,int yy) {
            return Noise(unsigned(xx+2048),unsigned(yy+2048),seed);
        };
        const float a=n(ix,iy)*(1-fx)+n(ix+1,iy)*fx;
        const float b=n(ix,iy+1)*(1-fx)+n(ix+1,iy+1)*fx;
        return a*(1-fy)+b*fy;
    }
    static const wi::graphics::Texture& Texture(unsigned kind) {
        static std::array<wi::graphics::Texture,Count> cached;
        auto& texture=cached[kind%Count];
        auto* device=wi::graphics::GetDevice();
        if(texture.IsValid()||!device)return texture;
        constexpr unsigned N=96;
        std::array<std::uint8_t,N*N*4> pixels{};
        for(unsigned y=0;y<N;++y)for(unsigned x=0;x<N;++x) {
            const float u=(x+.5f)/float(N)*2.f-1.f;
            const float v=(y+.5f)/float(N)*2.f-1.f;
            const float angle=std::atan2(v,u);
            const float radius=std::sqrt(u*u+v*v);
            const float large=SmoothNoise(u*3.4f+6.f,v*3.4f+8.f,kind+2);
            const float detail=SmoothNoise(u*11.f+11.f,v*11.f+5.f,kind+12);
            const float lobes=.11f*std::sin(angle*5.f+large*3.f);
            const float boundary=.72f+.25f*(large-.5f)+.10f*(detail-.5f)+lobes;
            const float feather=(kind==Fire?.18f:.24f);
            const float shape=std::clamp((boundary-radius)/feather,0.f,1.f);
            const float cavity=kind==Soot || kind==Dust
                ?(.35f+.65f*large)*(.5f+.5f*detail):(.74f+.26f*detail);
            const float alpha=std::clamp(shape*shape*(3.f-2.f*shape)*cavity,0.f,1.f);
            const float tone=.73f+.27f*detail;
            const unsigned index=(y*N+x)*4;
            if(kind==Fire){pixels[index]=255;pixels[index+1]=std::uint8_t(183*tone);pixels[index+2]=std::uint8_t(76*tone);}
            else {pixels[index]=pixels[index+1]=pixels[index+2]=std::uint8_t(255*tone);}
            pixels[index+3]=std::uint8_t(alpha*255.f);
        }
        wi::graphics::TextureDesc desc;
        desc.width=desc.height=N;desc.format=wi::graphics::Format::R8G8B8A8_UNORM;
        desc.bind_flags=wi::graphics::BindFlag::SHADER_RESOURCE;
        wi::graphics::SubresourceData data;
        data.data_ptr=pixels.data();data.row_pitch=N*4;data.slice_pitch=pixels.size();
        device->CreateTexture(&desc,&data,&texture);
        return texture;
    }
    static std::vector<Burst> Create(wi::scene::Scene& scene,const XMFLOAT3& position) {
        std::vector<Burst> out;out.reserve(Count);
        struct Recipe {
            Layer layer;float size;unsigned particles;float particleLife;
            float linger;XMFLOAT3 offset,velocity,gravity,color;
            float alpha,spread,scaleX,scaleY;
        };
        // Restrained flame core, turbulent shell, expanding dust and a few
        // ballistic incandescent flecks. Distinct time/scale layers avoid
        // the old single cluster of white-orange circular particles.
        constexpr std::array<Recipe,Count> recipes{{
            {Fire,.70f,16,.20f,.38f,{0,.14f,0},{0,1.3f,0},{0,-1.1f,0},{1.f,.64f,.18f},.95f,1.0f,.78f,1.15f},
            {Shock,1.1f,18,.24f,.45f,{0,.12f,0},{0,.25f,0},{0,-.6f,0},{.62f,.28f,.11f},.65f,2.4f,2.0f,.48f},
            {Soot,.83f,36,1.15f,1.50f,{0,.25f,0},{0,1.7f,0},{0,.18f,0},{.19f,.17f,.15f},.62f,1.8f,1.45f,1.35f},
            {Dust,.87f,30,.85f,1.25f,{0,.055f,0},{0,.55f,0},{0,-.2f,0},{.34f,.30f,.25f},.52f,2.7f,1.75f,.65f},
            {Embers,.055f,28,.53f,.90f,{0,.20f,0},{0,2.6f,0},{0,-7.2f,0},{1.f,.51f,.12f},.9f,3.1f,.50f,1.5f}
        }};
        for(const auto& recipe:recipes) {
            bridge::ProjectileEffectLayer request;
            request.kind=recipe.layer==Soot||recipe.layer==Dust
                ?bridge::ProjectileEffectKind::Smoke:bridge::ProjectileEffectKind::Sparks;
            request.sizeMetres=recipe.size;
            request.particleLifeSeconds=recipe.particleLife;
            request.particlesPerSecond=1;
            const XMFLOAT3 at{position.x+recipe.offset.x,
                position.y+recipe.offset.y,position.z+recipe.offset.z};
            const auto entity=bridge::CreateProjectileEffectEmitter(scene,request,at,true);
            if(entity==wi::ecs::INVALID_ENTITY)continue;
            if(auto* emitter=scene.emitters.GetComponent(entity)){
                emitter->burst_on_create=recipe.particles;
                emitter->SetMaxParticleCount(recipe.particles+4);
                emitter->count=0;
                emitter->random_factor=recipe.spread;
                emitter->velocity=recipe.velocity;
                emitter->gravity=recipe.gravity;
                emitter->scaleX=recipe.scaleX;emitter->scaleY=recipe.scaleY;
                emitter->random_life=.25f;
            }
            if(auto* material=scene.materials.GetComponent(entity)) {
                material->userBlendMode=recipe.layer==Fire||recipe.layer==Embers
                    ?wi::enums::BLENDMODE_ADDITIVE:wi::enums::BLENDMODE_ALPHA;
                material->baseColor={recipe.color.x,recipe.color.y,
                                     recipe.color.z,recipe.alpha};
                material->emissiveColor=recipe.layer==Fire||recipe.layer==Embers
                    ?XMFLOAT4{.7f,.3f,.06f,1.2f}:XMFLOAT4{0,0,0,0};
                const auto& mask=Texture(unsigned(recipe.layer));
                if(mask.IsValid())
                    material->textures[wi::scene::MaterialComponent::BASECOLORMAP].resource.SetTexture(mask);
                material->SetDirty();
            }
            out.push_back({entity,recipe.linger});
        }
        return out;
    }
};
}
