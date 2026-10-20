#pragma once
// P8 isolated research: realistic, short-lived natural lightning contacts.
// Visual-only. Does not alter equipment, collision, damage or the live Storm.
#include "RuntimeLightningStage3.h"
#include "RuntimeLightningRefinement3.h"
#include <array>
#include <cmath>
#include <fstream>
#include <algorithm>
#include <iostream>
#include <vector>

namespace renegade::runtime {
struct NaturalStorm {
    static constexpr unsigned Count=8, Frames=6, Width=512, Height=768, GroundSize=512;
    using Image=std::vector<unsigned char>;
    struct Slot {
        wi::ecs::Entity bolt=wi::ecs::INVALID_ENTITY;
        wi::ecs::Entity ground=wi::ecs::INVALID_ENTITY;
        wi::ecs::Entity light=wi::ecs::INVALID_ENTITY;
        wi::ecs::Entity sparks=wi::ecs::INVALID_ENTITY;
        float start=0, duration=.16f;
        XMFLOAT3 location{};
    };
    std::array<Slot,Count> slots{};
    wi::ecs::Entity root=wi::ecs::INVALID_ENTITY;
    bool driven=false;
    RuntimeLightningRefinement3 refinement3;
    float liveClock=0.f;
    unsigned liveLands=0, visibleTicks=0;
    XMFLOAT3 lastLand{};
    std::array<std::array<wi::graphics::Texture,Frames>,Count> lightning{};
    std::array<wi::graphics::Texture,Count> ground{};
    static unsigned Mix(unsigned v) {
        v^=v>>16; v*=0x7feb352du; v^=v>>15; v*=0x846ca68bu; return v^(v>>16);
    }
    static float Rand(unsigned seed) {return float(Mix(seed)&65535u)/65535.f;}
    static void Dot(Image& a,unsigned w,unsigned h,float x,float y,float width,float amount) {
        if(width<=0||amount<.004f)return;
        const float rad=std::max(2.2f,width*6.3f);
        for(int iy=std::max(0,int(y-rad));iy<=std::min(int(h)-1,int(y+rad));++iy)
        for(int ix=std::max(0,int(x-rad));ix<=std::min(int(w)-1,int(x+rad));++ix) {
            const float d=std::hypot(float(ix)-x,float(iy)-y);
            const float centre=std::exp(-d*d/std::max(.28f,width*width*.49f));
            const float edge=std::exp(-d*d/std::max(1.f,width*width*7.8f));
            const float halo=std::exp(-d*d/std::max(1.f,width*width*23.f));
            const float v=std::clamp(amount*(centre*.88f+edge*.43f+halo*.21f),0.f,1.f);
            const size_t k=(size_t(iy)*w+size_t(ix))*4;
            if(v*255.f<=a[k+3])continue;
            a[k]=static_cast<unsigned char>(std::clamp(37.f+218.f*std::min(1.f,centre*amount*1.8f),0.f,255.f));
            a[k+1]=static_cast<unsigned char>(std::clamp(89.f+166.f*std::min(1.f,centre*amount*1.65f),0.f,255.f));
            a[k+2]=255;
            a[k+3]=static_cast<unsigned char>(v*255.f);
        }
    }
    static void Segment(Image& a,unsigned w,unsigned h,float ax,float ay,
                        float bx,float by,float width,float power) {
        const float dist=std::hypot(bx-ax,by-ay);
        const unsigned n=std::max(1u,unsigned(std::ceil(dist*.9f)));
        for(unsigned i=0;i<=n;++i) {
            const float t=float(i)/float(n);
            Dot(a,w,h,ax+(bx-ax)*t,ay+(by-ay)*t,width,power);
        }
    }
    static Image BoltPixels(unsigned strike,unsigned frame) {
        Image p(size_t(Width)*Height*4,0);
        // Real discharge: a complete abrupt flash, fast return strokes and new
        // stochastic forks. No slow top-to-bottom growth or rigid beam.
        const float intensity[Frames]={.94f,1.f,.81f,.98f,.61f,.24f};
        const float power=intensity[frame];
        const unsigned seed=1007u+strike*509u+frame*43u;
        const unsigned n=34;
        std::array<float,n+1> xs{},ys{};
        xs[0]=256.f+(Rand(seed+10u)-.5f)*32.f;ys[0]=8.f;
        for(unsigned i=1;i<=n;++i){
            const float u=float(i)/n;
            xs[i]=std::clamp(xs[i-1]+(Rand(seed+i*29u)-.5f)*61.f
                       +(256.f-xs[i-1])*.085f,133.f,379.f);
            ys[i]=8.f+float(Height-22)*u;
        }
        for(unsigned i=0;i<n;++i) {
            const float t=float(i)/n;
            // Luminous narrow core with broader but subtle blue corona.
            Segment(p,Width,Height,xs[i],ys[i],xs[i+1],ys[i+1],
                    4.0f-(1.f-t)*.7f,power*(.89f+.11f*t));
            if(i>2 && i<n-2 && Rand(seed+1009u+i*23u)>.74f){
                float bx=xs[i],by=ys[i];
                const float dir=Rand(seed+440u+i*31u)>.5f?1.f:-1.f;
                const float reach=35.f+82.f*Rand(seed+121u+i*73u);
                const float droop=26.f+95.f*Rand(seed+4411u+i*83u);
                for(unsigned k=0;k<4;++k){
                    const float v=float(k+1)/4;
                    const float ex=xs[i]+dir*reach*v+(Rand(seed+i*37u+k*71u)-.5f)*23.f;
                    const float ey=std::min(float(Height-5),ys[i]+droop*v);
                    Segment(p,Width,Height,bx,by,ex,ey,(1.9f-.34f*k)*(.8f+.2f*(1.f-v)),
                            power*.7f*(1.f-.19f*k));
                    if(k==1 && Rand(seed+i*341u)>.51f) {
                        Segment(p,Width,Height,ex,ey,ex-dir*(24.f+Rand(seed+k+i)*38.f),
                                ey+27.f+Rand(seed+i*131u)*45.f,.65f,power*.36f);
                    }
                    bx=ex;by=ey;
                }
            }
        }
        return p;
    }
    static Image GroundPixels(unsigned strike){
        Image p(size_t(GroundSize)*GroundSize*4,0);
        const float mid=255.5f;
        const unsigned seed=916u+strike*347u;
        // Surface streamers wander along multiple independent uneven routes,
        // bifurcate, and die out: never a compass rose or segmented marker.
        for(unsigned arm=0;arm<8;++arm) {
            const float az=Rand(seed+arm*41u)*XM_2PI;
            const float total=58.f+157.f*Rand(seed+arm*83u);
            const unsigned parts=7u+unsigned(Rand(seed+arm*151u)*5.f);
            float x=mid+(Rand(seed+arm*17u)-.5f)*36.f;
            float y=mid+(Rand(seed+arm*13u)-.5f)*36.f;
            const float power=.76f+.23f*Rand(seed+arm*157u);
            for(unsigned step=0;step<parts;++step) {
                const float t=float(step+1)/parts;
                const float angle=az+(Rand(seed+arm*200u+step*127u)-.5f)*1.25f;
                const float reach=total/parts*(.67f+.6f*Rand(seed+arm*19u+step*83u));
                const float nx=x+std::cos(angle)*reach;
                const float ny=y+std::sin(angle)*reach;
                Segment(p,GroundSize,GroundSize,x,y,nx,ny,
                        std::max(.6f,3.35f*(1.f-t*.78f)),power*(1.f-t*.62f));
                if(step>=2 && step<=5 && Rand(seed+arm*47u+step*99u)>.47f){
                    const float angle2=angle+(Rand(seed+arm*397u+step*21u)>.5f?.57f:-.63f);
                    const float len=17.f+29.f*Rand(seed+arm*7u+step*59u);
                    Segment(p,GroundSize,GroundSize,x,y,
                        x+std::cos(angle2)*len,y+std::sin(angle2)*len,1.25f,power*.44f*(1.f-t));
                }
                x=nx;y=ny;
            }
        }
        return p;
    }
    static bool Upload(const Image& p,unsigned w,unsigned h,wi::graphics::Texture& result){
        auto* dev=wi::graphics::GetDevice();if(!dev)return false;
        wi::graphics::TextureDesc desc;desc.width=w;desc.height=h;
        desc.format=wi::graphics::Format::R8G8B8A8_UNORM;
        desc.bind_flags=wi::graphics::BindFlag::SHADER_RESOURCE;
        wi::graphics::SubresourceData data;data.data_ptr=p.data();
        data.row_pitch=w*4;data.slice_pitch=unsigned(p.size());
        return dev->CreateTexture(&desc,&data,&result);
    }
    static wi::ecs::Entity Card(wi::scene::Scene& scene,const char* name,float glow){
        const auto e=scene.Entity_CreateCube(name);
        if(auto* m=scene.materials.GetComponent(e)){
            m->baseColor={1,1,1,1};
            m->emissiveColor={.18f,.42f,1.f,glow};
            m->userBlendMode=wi::enums::BLENDMODE_ADDITIVE;
            m->SetDoubleSided(true);m->SetCastShadow(false);m->SetDirty();
        }
        if(auto* o=scene.objects.GetComponent(e)){
            o->filterMask=wi::enums::FILTER_TRANSPARENT;o->SetCastShadow(false);
        }
        return e;
    }
    static void Move(wi::scene::Scene& scene,wi::ecs::Entity e,
                     XMFLOAT3 position,float sx,float sy,float sz){
        if(auto* t=scene.transforms.GetComponent(e)){
            t->ClearTransform();t->Scale(XMFLOAT3{sx,sy,sz});
            t->Translate(position);t->UpdateTransform();
        }
    }
    static void Put(wi::scene::Scene& scene,wi::ecs::Entity e,const wi::graphics::Texture& tx){
        if(auto* m=scene.materials.GetComponent(e)){
            m->textures[wi::scene::MaterialComponent::BASECOLORMAP].resource.SetTexture(tx);
            m->SetDirty();
        }
    }
    bool Create(wi::scene::Scene& scene) {
        if(root!=wi::ecs::INVALID_ENTITY)return true;
        // Fail early rather than silently showing untextured rectangular cards.
        for(unsigned s=0;s<Count;++s){
            for(unsigned f=0;f<Frames;++f)
                if(!Upload(BoltPixels(s,f),Width,Height,lightning[s][f]))return false;
            if(!Upload(GroundPixels(s),GroundSize,GroundSize,ground[s]))return false;
        }
        root=scene.Entity_CreateTransform("Natural lightning visual-only root");
        constexpr std::array<float,Count> when{.31f,.37f,.41f,.49f,1.17f,1.23f,1.29f,1.38f};
        constexpr std::array<XMFLOAT3,Count> positions{{
            {0,0,4.5f},{-1.35f,0,4.3f},{1.25f,0,5.15f},{.63f,0,3.45f},
            {-1.8f,0,5.45f},{1.58f,0,3.9f},{-.52f,0,5.85f},{1.9f,0,5.0f}
        }};
        for(unsigned s=0;s<Count;++s){
            Slot& b=slots[s];
            b.start=when[s];b.duration=.14f+.065f*Rand(s+50u);b.location=positions[s];
            b.bolt=Card(scene,"NATURAL downward branched return stroke",2.6f);
            b.ground=Card(scene,"NATURAL ground-crawling electrical streamers",2.4f);
            Put(scene,b.bolt,lightning[s][0]);Put(scene,b.ground,ground[s]);
            b.light=scene.Entity_CreateLight("NATURAL genuine ground-contact point light",{},
                XMFLOAT3{.68f,.82f,1.f},0.f,7.2f,wi::scene::LightComponent::POINT);
            b.sparks=bridge::CreateProjectileEffectEmitter(scene,
                {bridge::ProjectileEffectKind::Sparks,{},.022f,0.f,.18f},{});
            if(b.sparks==wi::ecs::INVALID_ENTITY)return false;
            if(auto* em=scene.emitters.GetComponent(b.sparks)){
                em->SetMaxParticleCount(200);em->life=.13f;em->random_life=.28f;
                em->velocity={0,2.7f,0};em->gravity={0,-9.8f,0};
                em->random_factor=1.1f;em->normal_factor=.9f;em->count=0;
                em->SetCollidersDisabled(false);
            }
            if(auto* m=scene.materials.GetComponent(b.sparks)){
                m->baseColor={.72f,.86f,1.f,.85f};
                m->emissiveColor={.68f,.83f,1.f,2.5f};m->SetDirty();
            }
            for(auto e:{b.bolt,b.ground,b.light,b.sparks})
                scene.Component_Attach(e,root,true);
            Move(scene,b.light,{b.location.x,.25f,b.location.z},1,1,1);
            Move(scene,b.sparks,{b.location.x,.08f,b.location.z},1,1,1);
            Move(scene,b.bolt,{0,0,0},.00001f,.00001f,.00001f);
            Move(scene,b.ground,{0,0,0},.00001f,.00001f,.00001f);
        }
        return true;
    }
    void Update(wi::scene::Scene& scene,float time){
        for(unsigned s=0;s<Count;++s){
            Slot& b=slots[s];
            const float age=time-b.start;
            const bool active=age>=0.f&&age<b.duration;
            if(auto* l=scene.lights.GetComponent(b.light)) {
                l->intensity=active?39.f*std::exp(-age*11.f)*(age<.045f?1.f:.58f):0.f;
            }
            if(auto* em=scene.emitters.GetComponent(b.sparks))
                em->count=active && age<.048f?175.f:0.f;
            if(active){
                ++visibleTicks;
                const unsigned pose=std::min(Frames-1,unsigned(age*60.f));
                Put(scene,b.bolt,lightning[s][pose]);
                Move(scene,b.bolt,{b.location.x,b.location.y+2.78f,b.location.z},2.15f,2.8f,.0015f);
            }else Move(scene,b.bolt,{0,0,0},.00001f,.00001f,.00001f);
            const float groundTime=.19f;
            const bool surface=age>=0.f&&age<groundTime;
            if(surface){
                const float attenuation=std::clamp((groundTime-age)/groundTime,0.f,1.f);
                Move(scene,b.ground,{b.location.x,b.location.y+.057f,b.location.z},
                     3.25f*std::sqrt(attenuation),.0015f,3.25f*std::sqrt(attenuation));
            }else Move(scene,b.ground,{0,0,0},.00001f,.00001f,.00001f);
        }
    }
    // Opt-in only: the runtime gameplay scheduler owns strike positions and
    // damage. These entities have no collision or independent hit events.
    bool BeginLive(wi::scene::Scene& scene){
        if(!refinement3.Create(scene))return false;
        refinement3.Begin(scene);
        driven=true;liveClock=0.f;
        for(auto& b:slots){
            b.start=10000.f;
            Move(scene,b.bolt,{0,0,0},.00001f,.00001f,.00001f);
            Move(scene,b.ground,{0,0,0},.00001f,.00001f,.00001f);
            if(auto* l=scene.lights.GetComponent(b.light))l->intensity=0.f;
            if(auto* em=scene.emitters.GetComponent(b.sparks))em->count=0.f;
        }
        return true;
    }
    void Land(wi::scene::Scene& scene,const XMFLOAT3& at,unsigned index){
        if(!driven)return;
        auto& b=slots[index%Count];
        b.location=at;b.start=liveClock;
        lastLand=at;++liveLands;
        refinement3.Land(scene,at,index);
        Move(scene,b.light,{at.x,at.y+.25f,at.z},1.f,1.f,1.f);
        Move(scene,b.sparks,{at.x,at.y+.08f,at.z},1.f,1.f,1.f);
    }
    void TickLive(wi::scene::Scene& scene,float dt){
        if(!driven||!std::isfinite(dt)||dt<=0.f)return;
        liveClock+=dt;
        // Never reactivate the old planar research cards in the playable scene.
        refinement3.Update(scene,dt);
    }
    void Travel(wi::scene::Scene& scene,unsigned index,const XMFLOAT3& at,float fraction){
        if(driven)refinement3.Travel(scene,index,at,fraction);
    }
    void Reset(wi::scene::Scene& scene){
        refinement3.Reset(scene);
        if(root!=wi::ecs::INVALID_ENTITY)scene.Entity_Remove(root);
        *this={};
    }
};
} // namespace renegade::runtime
