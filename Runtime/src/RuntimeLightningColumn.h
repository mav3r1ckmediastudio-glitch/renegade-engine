#pragma once
// Isolated owner review only. No Storm scheduler, combat or approved VFX changes.
#include "RuntimeChannelVisual.h"
#include <array>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <string>
#include <vector>

namespace renegade::runtime {
struct RuntimeLightningColumn {
    static constexpr unsigned Frames=11, Strips=6;
    wi::ecs::Entity root=wi::ecs::INVALID_ENTITY;
    std::array<wi::ecs::Entity,2> core{};
    std::array<wi::ecs::Entity,Strips> strips{};
    std::array<wi::Resource,Frames> frames{};
    std::array<unsigned,Strips> lastFrame{};
    std::array<unsigned,Strips> visited{};
    wi::graphics::Texture columnTexture{},contactTexture{};
    wi::ecs::Entity source=wi::ecs::INVALID_ENTITY,sourceHalo=wi::ecs::INVALID_ENTITY;
    wi::ecs::Entity contact=wi::ecs::INVALID_ENTITY,contactHalo=wi::ecs::INVALID_ENTITY;
    wi::ecs::Entity sourceLight=wi::ecs::INVALID_ENTITY,impactLight=wi::ecs::INVALID_ENTITY;
    wi::ecs::Entity sparks=wi::ecs::INVALID_ENTITY;
    float time=0;
    bool failed=false;

    static bool Upload(const std::vector<unsigned char>& bytes,unsigned w,unsigned h,
                       wi::graphics::Texture& texture) {
        auto* dev=wi::graphics::GetDevice(); if(!dev)return false;
        wi::graphics::TextureDesc desc;desc.width=w;desc.height=h;
        desc.format=wi::graphics::Format::R8G8B8A8_UNORM;
        desc.bind_flags=wi::graphics::BindFlag::SHADER_RESOURCE;
        wi::graphics::SubresourceData data;
        data.data_ptr=bytes.data();data.row_pitch=w*4;data.slice_pitch=unsigned(bytes.size());
        return dev->CreateTexture(&desc,&data,&texture);
    }
    static std::vector<unsigned char> GlowPixels(bool radial) {
        constexpr unsigned W=256,H=256;
        std::vector<unsigned char> bytes(W*H*4);
        for(unsigned y=0;y<H;++y)for(unsigned x=0;x<W;++x) {
            const float u=(float(x)+.5f-128.f)/128.f,v=(float(y)+.5f-128.f)/128.f;
            const float r=radial?std::hypot(u,v):std::abs(u);
            const float white=std::exp(-r*r/(radial?.024f:.0045f));
            const float blue=std::exp(-r*r/(radial?.12f:.09f));
            const float edge=radial?1.f:std::clamp((1.f-std::abs(v))*24.f,0.f,1.f);
            const size_t p=(size_t(y)*W+x)*4;
            bytes[p]=static_cast<unsigned char>(255*std::clamp((white+.06f*blue)*edge,0.f,1.f));
            bytes[p+1]=static_cast<unsigned char>(255*std::clamp((white+.21f*blue)*edge,0.f,1.f));
            bytes[p+2]=static_cast<unsigned char>(255*std::clamp((white+.38f*blue)*edge,0.f,1.f));
            bytes[p+3]=255; // RGB drives additive energy; black contributes exactly zero.
        }
        return bytes;
    }
    static void Material(wi::scene::Scene& scene,wi::ecs::Entity e,float emission) {
        if(auto* m=scene.materials.GetComponent(e)) {
            m->shaderType=wi::scene::MaterialComponent::SHADERTYPE_UNLIT;
            m->baseColor={1,1,1,1};m->emissiveColor={1,1,1,emission};
            m->userBlendMode=wi::enums::BLENDMODE_ADDITIVE;
            m->SetDoubleSided(true);m->SetCastShadow(false);m->SetRoughness(1);
            m->SetReflectance(0);m->SetDirty();
        }
        if(auto* o=scene.objects.GetComponent(e)) {
            o->filterMask=wi::enums::FILTER_TRANSPARENT;o->SetCastShadow(false);
        }
    }
    wi::ecs::Entity Quad(wi::scene::Scene& scene,const char* name,float emission) {
        const auto e=scene.Entity_CreatePlane(name);
        // Genuine two-triangle XY quad: no cube backface doubling or thin side faces.
        if(auto* m=scene.meshes.GetComponent(e)) {
            m->vertex_positions={{-1,1,0},{-1,-1,0},{1,-1,0},{1,1,0}};
            m->vertex_normals={{0,0,-1},{0,0,-1},{0,0,-1},{0,0,-1}};
            m->CreateRenderData();
        }
        scene.Component_Attach(e,root,true);Material(scene,e,emission);return e;
    }
    static void Bind(wi::scene::Scene& scene,wi::ecs::Entity e,const wi::graphics::Texture& t) {
        if(auto* m=scene.materials.GetComponent(e)) {
            // Mask emission too: constant emissive colour would illuminate the entire rectangle.
            m->textures[wi::scene::MaterialComponent::BASECOLORMAP].resource.SetTexture(t);
            m->textures[wi::scene::MaterialComponent::EMISSIVEMAP].resource.SetTexture(t);
            m->SetDirty();
        }
    }
    static void Place(wi::scene::Scene& scene,wi::ecs::Entity e,const XMFLOAT3& p,
                      const XMFLOAT3& scale,const XMVECTOR& q=XMQuaternionIdentity()) {
        if(auto* t=scene.transforms.GetComponent(e)) {
            t->ClearTransform();t->Scale(scale);
            XMFLOAT4 rotation;XMStoreFloat4(&rotation,q);t->Rotate(rotation);
            t->Translate(p);t->UpdateTransform();
        }
    }
    bool Create(wi::scene::Scene& scene,const std::string& assetFolder) {
        if(root!=wi::ecs::INVALID_ENTITY)return true;
        if(failed)return false;
        failed=true;
        for(unsigned i=0;i<Frames;++i) {
            const auto file=(std::filesystem::u8path(assetFolder)/(std::to_string(i)+".png")).generic_u8string();
            frames[i]=wi::resourcemanager::Load(file);
            if(!frames[i].IsValid()||!frames[i].GetTexture().IsValid())return false;
        }
        if(!Upload(GlowPixels(false),256,256,columnTexture)||
           !Upload(GlowPixels(true),256,256,contactTexture))return false;
        root=scene.Entity_CreateTransform("Isolated supplied-strip lightning COLUMN");
        for(auto& e:core){e=Quad(scene,"Pulsing white column soft core",7.f);Bind(scene,e,columnTexture);}
        for(auto& e:strips)e=Quad(scene,"Owner 11-frame animated electrical strip",2.8f);
        lastFrame.fill(Frames);
        source=scene.Entity_CreateSphere("Column charged white source sphere");
        sourceHalo=scene.Entity_CreateSphere("Column charged cyan source shell");
        scene.Component_Attach(source,root,true);scene.Component_Attach(sourceHalo,root,true);
        Material(scene,source,5.f);Material(scene,sourceHalo,.7f);
        if(auto* m=scene.materials.GetComponent(sourceHalo)){
            m->baseColor={.15f,.4f,1.f,.08f};m->emissiveColor={.12f,.34f,1.f,.28f};m->SetDirty();
        }
        contact=Quad(scene,"Soft white ground contact (no star or diamond)",5.f);
        contactHalo=Quad(scene,"Diffuse cyan ground contact",1.3f);
        Bind(scene,contact,contactTexture);Bind(scene,contactHalo,contactTexture);
        sourceLight=scene.Entity_CreateLight("Column source POINT illumination",{},XMFLOAT3{.35f,.65f,1},3.f,3.5f,wi::scene::LightComponent::POINT);
        impactLight=scene.Entity_CreateLight("Column ORIGINAL real impact POINT LIGHT",{},XMFLOAT3{.66f,.82f,1},19.f,4.2f,wi::scene::LightComponent::POINT);
        scene.Component_Attach(sourceLight,root,true);scene.Component_Attach(impactLight,root,true);
        sparks=bridge::CreateProjectileEffectEmitter(scene,
            {bridge::ProjectileEffectKind::Sparks,{},.033f,0.f,.38f},{});
        if(sparks==wi::ecs::INVALID_ENTITY){Reset(scene);failed=true;return false;}
        scene.Component_Attach(sparks,root,true);
        if(auto* e=scene.emitters.GetComponent(sparks)) {
            e->SetMaxParticleCount(280);e->life=.28f;e->random_life=.45f;
            e->velocity={0,3.4f,0};e->gravity={0,-8.6f,0};
            e->random_factor=3.f;e->normal_factor=2.4f;e->count=180;
            e->motionBlurAmount=.18f;
            e->SetCollidersDisabled(false);
        }
        if(auto* m=scene.materials.GetComponent(sparks)){
            m->baseColor={.72f,.88f,1.f,.95f};m->emissiveColor={.7f,.86f,1.f,3.f};m->SetDirty();
        }
        failed=false;return true;
    }
    void Update(wi::scene::Scene& scene,const XMFLOAT3& ground,float dt) {
        if(root==wi::ecs::INVALID_ENTITY||!std::isfinite(dt)||dt<0)return;
        time+=dt;
        const float pulse=.5f+.5f*std::sin(time*24.f)+.16f*std::sin(time*63.f);
        const float width=.85f+.22f*pulse,height=5.56f;
        const XMFLOAT3 middle{ground.x,ground.y+height*.5f,ground.z};
        for(unsigned i=0;i<core.size();++i) {
            Place(scene,core[i],middle,{width,height*.5f,1},
                  XMQuaternionRotationRollPitchYaw(0,i*XM_PIDIV2,0));
            if(auto* m=scene.materials.GetComponent(core[i])) {
                m->emissiveColor={1,1,1,5.5f+3.f*pulse};m->SetDirty();
            }
        }
        for(unsigned i=0;i<Strips;++i) {
            const unsigned f=unsigned(time*(23.f+i*1.7f)+i*2)%Frames;
            visited[i]|=1u<<f;
            if(lastFrame[i]!=f){Bind(scene,strips[i],frames[f].GetTexture());lastFrame[i]=f;}
            const float a=i*XM_2PI/Strips+time*(i%2?.24f:-.17f);
            const float radius=.25f+.10f*std::sin(time*7.f+i*2.1f);
            const XMFLOAT3 p{middle.x+radius*std::cos(a),middle.y+.07f*std::sin(time*9.f+i),middle.z+radius*std::sin(a)};
            const auto turn=XMQuaternionRotationRollPitchYaw(0,a,XM_PIDIV2+.035f*std::sin(time*11.f+i));
            Place(scene,strips[i],p,{2.58f,.45f+.10f*std::sin(time*8.f+i),1},turn);
        }
        const XMFLOAT3 top{ground.x,ground.y+height,ground.z};
        const float sourceSize=.17f+.025f*pulse;
        Place(scene,source,top,{sourceSize,sourceSize,sourceSize});
        Place(scene,sourceHalo,top,{.27f,.27f,.27f});
        Place(scene,sourceLight,top,{1,1,1});
        if(auto* l=scene.lights.GetComponent(sourceLight))l->intensity=2.5f+pulse;
        const auto flat=XMQuaternionRotationRollPitchYaw(XM_PIDIV2,0,0);
        Place(scene,contact,{ground.x,ground.y+.028f,ground.z},{.62f+.12f*pulse,.62f+.12f*pulse,1},flat);
        Place(scene,contactHalo,{ground.x,ground.y+.024f,ground.z},{1.1f,1.1f,1},flat);
        Place(scene,impactLight,{ground.x,ground.y+.34f,ground.z},{1,1,1});
        if(auto* l=scene.lights.GetComponent(impactLight))l->intensity=15.f+8.f*pulse;
        Place(scene,sparks,{ground.x,ground.y+.055f,ground.z},{1,1,1});
        if(auto* e=scene.emitters.GetComponent(sparks))e->count=140.f+80.f*pulse;
    }
    void Reset(wi::scene::Scene& scene) {
        if(root!=wi::ecs::INVALID_ENTITY)scene.Entity_Remove(root);
        *this={};
    }
};
}
