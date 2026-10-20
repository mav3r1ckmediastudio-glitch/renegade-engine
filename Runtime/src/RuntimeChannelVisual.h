#pragma once
#include "renegade/bridge/ProjectileEffectRuntime.h"
#include <WickedEngine.h>
#include <algorithm>
#include <array>
#include <cmath>

namespace renegade::runtime {
// Playable provisional beam: thin contact-accurate core, travelling braided energy
// and native GPU glints. All geometry is presentation only, under one excluded root.
struct RuntimeChannelVisual {
    static constexpr unsigned Filaments=3, Segments=12, PulseCount=16, ShedEmitters=6;
    wi::ecs::Entity root=wi::ecs::INVALID_ENTITY;
    wi::ecs::Entity core=wi::ecs::INVALID_ENTITY;
    std::array<std::array<wi::ecs::Entity,Segments>,Filaments> strands{};
    std::array<std::array<wi::ecs::Entity,2>,PulseCount> pulses{};
    wi::ecs::Entity palm=wi::ecs::INVALID_ENTITY,tip=wi::ecs::INVALID_ENTITY;
    wi::ecs::Entity sourceSparks=wi::ecs::INVALID_ENTITY,travelSparks=wi::ecs::INVALID_ENTITY;
    std::array<wi::ecs::Entity,ShedEmitters> shedSparks{};
    wi::ecs::Entity contactSparks=wi::ecs::INVALID_ENTITY,impactEmbers=wi::ecs::INVALID_ENTITY;
    wi::ecs::Entity contactLight=wi::ecs::INVALID_ENTITY;
    float seconds=0;
    static float PulseDistance(float seconds,unsigned index,float length) noexcept {
        if(!std::isfinite(seconds)||!std::isfinite(length)||length<=0||index>=PulseCount)return -1.f;
        // Constant speed in metres per second; pulse spacing adapts to contact distance.
        constexpr float speed=17.f, rearGap=1.5f;
        const float cycle=length+rearGap;
        const unsigned slots=std::clamp(unsigned(std::ceil(length/1.8f))+1u,2u,PulseCount);
        if(index>=slots)return -1.f;
        float metres=std::fmod(seconds*speed-float(index)*cycle/float(slots),cycle);
        if(metres<0)metres+=cycle;
        return metres<=length?metres:-1.f;
    }
    void Reset(wi::scene::Scene& scene) {
        if(root!=wi::ecs::INVALID_ENTITY)scene.Entity_Remove(root);
        *this={};
    }
    static void Place(wi::scene::Scene& scene,wi::ecs::Entity entity,DirectX::FXMVECTOR pos,float radius) {
        if(auto* t=scene.transforms.GetComponent(entity)){
            XMFLOAT3 p;XMStoreFloat3(&p,pos);
            t->ClearTransform();t->Scale(XMFLOAT3{radius,radius,radius});
            t->Translate(p);t->UpdateTransform();
        }
    }
    static void Segment(wi::scene::Scene& scene,wi::ecs::Entity entity,
                        DirectX::FXMVECTOR a,DirectX::FXMVECTOR b,float radius) {
        auto* t=scene.transforms.GetComponent(entity);if(!t)return;
        const auto difference=b-a;
        const float length=XMVectorGetX(XMVector3Length(difference));
        if(length<.0001f)return;
        const auto forward=XMVector3Normalize(difference);
        const auto reference=std::abs(XMVectorGetY(forward))>.98f?XMVectorSet(1,0,0,0):XMVectorSet(0,1,0,0);
        const auto right=XMVector3Normalize(XMVector3Cross(reference,forward));
        const auto up=XMVector3Cross(forward,right);
        const auto q=XMQuaternionRotationMatrix(XMMATRIX(right,up,forward,XMVectorSet(0,0,0,1)));
        XMFLOAT4 rotation;XMStoreFloat4(&rotation,q);
        XMFLOAT3 center;XMStoreFloat3(&center,(a+b)*.5f);
        t->ClearTransform();t->Scale(XMFLOAT3{radius,radius,length*.5f});
        t->Rotate(rotation);t->Translate(center);t->UpdateTransform();
    }
    wi::ecs::Entity MakeGlow(wi::scene::Scene& scene,const char* name,XMFLOAT4 color,XMFLOAT4 emissive) {
        const auto entity=scene.Entity_CreateSphere(name);
        scene.Component_Attach(entity,root,true);
        if(auto* m=scene.materials.GetComponent(entity)){
            m->baseColor=color;m->emissiveColor=emissive;
            m->userBlendMode=wi::enums::BLENDMODE_ADDITIVE;m->SetDirty();
        }
        if(auto* object=scene.objects.GetComponent(entity))
            object->filterMask=wi::enums::FILTER_TRANSPARENT;
        return entity;
    }
    wi::ecs::Entity MakeSparks(wi::scene::Scene& scene,const char* name,float size,float rate) {
        const auto entity=bridge::CreateProjectileEffectEmitter(scene,
            {bridge::ProjectileEffectKind::Sparks,{},size,rate,.18f},{});
        scene.Component_Attach(entity,root,true);
        if(auto* e=scene.emitters.GetComponent(entity)){
            e->SetMaxParticleCount(128);e->random_factor=.65f;e->normal_factor=.15f;
            e->random_life=.25f;e->velocity={0,0,0};e->gravity={0,0,0};
            e->scaleX=.4f;e->scaleY=1.35f;e->rotation=1.2f;
        }
        if(auto* m=scene.materials.GetComponent(entity)){
            m->baseColor={.38f,.77f,1.f,.85f};
            m->emissiveColor={.35f,.7f,1.f,1.5f};m->SetDirty();
        }
        if(auto* n=scene.names.GetComponent(entity))n->name=name;
        return entity;
    }
    void Create(wi::scene::Scene& scene) {
        root=scene.Entity_CreateTransform("Channel energy (presentation only)");
        core=MakeGlow(scene,"Channel white-hot filament",{.7f,.94f,1.f,.95f},{.6f,.9f,1.f,2.5f});
        const std::array<XMFLOAT4,3> strandColors={
            XMFLOAT4{.56f,.84f,1.f,.42f},XMFLOAT4{.26f,.54f,.8f,.29f},XMFLOAT4{.78f,.91f,1.f,.38f}};
        for(unsigned i=0;i<Filaments;++i)for(unsigned j=0;j<Segments;++j)
            strands[i][j]=MakeGlow(scene,"Channel travelling arc",strandColors[i],
                {strandColors[i].x,strandColors[i].y,strandColors[i].z,1.3f});
        // Reusable luminous wavefronts: directional bursts, not decorative hoops.
        for(unsigned i=0;i<PulseCount;++i) {
            pulses[i][0]=MakeGlow(scene,"Channel travelling white pulse",
                {.83f,.94f,1.f,.9f},{.85f,.97f,1.f,3.2f});
            pulses[i][1]=MakeGlow(scene,"Channel travelling blue pulse wake",
                {.1f,.38f,.7f,.32f},{.18f,.46f,.72f,.65f});
        }
        palm=MakeGlow(scene,"Channel hand corona",{.35f,.8f,1.f,.5f},{.4f,.8f,1.f,1.8f});
        tip=MakeGlow(scene,"Channel contact corona",{.3f,.8f,1.f,.45f},{.5f,.7f,1.f,2.f});
        sourceSparks=MakeSparks(scene,"Channel hand particles",.018f,36);
        travelSparks=MakeSparks(scene,"Channel flowing particles",.028f,80);
        contactSparks=MakeSparks(scene,"Channel surface sparks",.016f,72);
        if(auto* e=scene.emitters.GetComponent(contactSparks)) {
            e->life=.36f;e->random_life=.55f;
            e->gravity={0,-7.8f,0};e->normal_factor=.85f;
            e->SetCollidersDisabled(false);
            e->scaleX=.25f;e->scaleY=1.8f;
            e->motionBlurAmount=.15f;
        }
        impactEmbers=MakeSparks(scene,"Channel cooling impact embers",.011f,30);
        if(auto* e=scene.emitters.GetComponent(impactEmbers)) {
            e->life=.7f;e->random_life=.4f;
            e->gravity={0,-9.8f,0};e->normal_factor=.9f;
            e->SetCollidersDisabled(false);
            e->scaleX=.2f;e->scaleY=1.1f;
        }
        if(auto* m=scene.materials.GetComponent(impactEmbers)){
            m->baseColor={.72f,.86f,1.f,.76f};
            m->emissiveColor={.36f,.64f,1.f,.8f};m->SetDirty();
        }
        // Multiple spatially distributed emitters shed discharge everywhere,
        // rather than moving a single particle source along the whole beam.
        for(unsigned i=0;i<ShedEmitters;++i) {
            shedSparks[i]=MakeSparks(scene,"Channel length discharge",.018f,45);
            if(auto* e=scene.emitters.GetComponent(shedSparks[i])) {
                e->life=.14f;e->random_life=.55f;e->random_factor=.8f;
                e->scaleX=.22f;e->scaleY=1.5f;
            }
        }
        contactLight=scene.Entity_CreateLight("Channel contact light",{},XMFLOAT3{.15f,.56f,1.f},0,1.6f,wi::scene::LightComponent::POINT);
        scene.Component_Attach(contactLight,root,true);
    }
    void Update(wi::scene::Scene& scene,XMFLOAT3 from,XMFLOAT3 to,bool contact,float dt,
                XMFLOAT3 hitNormal={}) {
        const auto a=XMLoadFloat3(&from),b=XMLoadFloat3(&to),delta=b-a;
        const float length=XMVectorGetX(XMVector3Length(delta));
        if(!std::isfinite(length)||length<.001f){Reset(scene);return;}
        if(root==wi::ecs::INVALID_ENTITY)Create(scene);
        if(std::isfinite(dt)&&dt>0)seconds+=std::min(dt,.1f);
        const auto forward=XMVector3Normalize(delta);
        const auto reference=std::abs(XMVectorGetY(forward))>.98f?XMVectorSet(1,0,0,0):XMVectorSet(0,1,0,0);
        const auto right=XMVector3Normalize(XMVector3Cross(reference,forward));
        const auto up=XMVector3Cross(forward,right);
        // No opaque blue tube: a hairline white core and energetic braided arcs.
        Segment(scene,core,a,b,.021f);
        constexpr float pi=3.14159265358979323846f;
        const auto filamentPoint=[&](unsigned strand,unsigned j) {
            const float u=float(j)/float(Segments);
            const float envelope=std::sin(u*pi);
            const float phase=seconds*12.f+u*23.f+strand*2.0943951f;
            const float x=std::sin(phase)*(.012f+.025f*envelope)+.012f*std::sin(u*71.f-seconds*19.f+strand);
            const float y=std::cos(phase*.83f+strand*.9f)*(.014f+.025f*envelope)+.009f*std::cos(u*83.f-seconds*15.f);
            return a+delta*u+(right*x+up*y)*(3.f*envelope);
        };
        for(unsigned i=0;i<Filaments;++i)for(unsigned j=0;j<Segments;++j)
            Segment(scene,strands[i][j],filamentPoint(i,j),filamentPoint(i,j+1),
                i==0?.018f:.0135f);
        // Each pulse travels from the hand to its actual contact distance at
        // a fixed speed, carrying a short bright leading head and a blue wake.
        for(unsigned i=0;i<PulseCount;++i) {
            const float metres=PulseDistance(seconds,i,length);
            if(metres<0) {
                Place(scene,pulses[i][0],a,0);
                Place(scene,pulses[i][1],a,0);
                continue;
            }
            const float lead=std::min(length,metres+.065f);
            const float tail=std::max(0.f,metres-.28f);
            const float wake=std::max(0.f,metres-.52f);
            if(lead>tail+.001f)Segment(scene,pulses[i][0],a+forward*tail,a+forward*lead,.051f);
            else Place(scene,pulses[i][0],a,0);
            if(lead>wake+.001f)Segment(scene,pulses[i][1],a+forward*wake,a+forward*lead,.081f);
            else Place(scene,pulses[i][1],a,0);
        }
        Place(scene,palm,a,.043f+.006f*std::sin(seconds*11.f));
        Place(scene,tip,b,contact?.065f+.013f*std::sin(seconds*17.f):.015f);
        const auto moveEmitter=[&](wi::ecs::Entity entity,DirectX::FXMVECTOR at,float rate) {
            if(auto* t=scene.transforms.GetComponent(entity)){
                XMFLOAT3 pos;XMStoreFloat3(&pos,at);
                t->ClearTransform();t->Translate(pos);t->UpdateTransform();
            }
            if(auto* e=scene.emitters.GetComponent(entity))e->count=rate;
        };
        moveEmitter(sourceSparks,a,35.f);
        const float advance=seconds*2.4f-std::floor(seconds*2.4f);
        moveEmitter(travelSparks,a+delta*(.02f+advance*.96f),76.f);
        for(unsigned i=0;i<ShedEmitters;++i) {
            // Uniform coverage including short beams; a moving radial vector
            // scatters sparks away from the stream without particle collisions.
            const float u=(float(i)+.5f)/float(ShedEmitters);
            const float angle=seconds*7.f+float(i)*2.399963f;
            const auto radial=right*std::cos(angle)+up*std::sin(angle);
            moveEmitter(shedSparks[i],a+delta*u,72.f);
            if(auto* e=scene.emitters.GetComponent(shedSparks[i])) {
                XMFLOAT3 v;
                XMStoreFloat3(&v,radial*1.8f+forward*.3f);
                e->velocity=v;
            }
        }
        auto surfaceNormal=XMLoadFloat3(&hitNormal);
        if(XMVectorGetX(XMVector3LengthSq(surfaceNormal))<.001f)
            surfaceNormal=-forward;
        surfaceNormal=XMVector3Normalize(surfaceNormal);
        const auto surfaceEmission=b+surfaceNormal*.018f;
        moveEmitter(contactSparks,surfaceEmission,contact?85.f:0.f);
        moveEmitter(impactEmbers,surfaceEmission,contact?28.f:0.f);
        if(contact) {
            XMFLOAT3 sparkVelocity,emberVelocity;
            XMStoreFloat3(&sparkVelocity,surfaceNormal*2.0f+XMVectorSet(0,1.15f,0,0));
            XMStoreFloat3(&emberVelocity,surfaceNormal*.75f+XMVectorSet(0,.55f,0,0));
            if(auto* e=scene.emitters.GetComponent(contactSparks))e->velocity=sparkVelocity;
            if(auto* e=scene.emitters.GetComponent(impactEmbers))e->velocity=emberVelocity;
        }
        if(auto* t=scene.transforms.GetComponent(contactLight)) {
            t->ClearTransform();t->Translate(to);t->UpdateTransform();
        }
        if(auto* light=scene.lights.GetComponent(contactLight)){
            light->intensity=contact?.055f*(1.f+.15f*std::sin(seconds*12.f)):0.f;
        }
    }
};
}
