#pragma once
// Refinement 3: prebuilt connected tube meshes, never one high-poly sphere per segment.
// Visual only. Storm scheduling, targeting and damage stay authoritative elsewhere.
#include "RuntimeChannelVisual.h"
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace renegade::runtime {
struct RuntimeLightningRefinement3 {
    static constexpr unsigned Visible=3, Strands=4, Pieces=24, Sides=6;
    static constexpr unsigned Poses=8, Stages=4, CachedPairs=Poses*Stages;
    struct Slot {
        wi::ecs::Entity core=wi::ecs::INVALID_ENTITY, aura=wi::ecs::INVALID_ENTITY;
        wi::ecs::Entity ground=wi::ecs::INVALID_ENTITY, sparks=wi::ecs::INVALID_ENTITY;
        wi::ecs::Entity contact=wi::ecs::INVALID_ENTITY;
        wi::ecs::Entity impactLight=wi::ecs::INVALID_ENTITY, skyLight=wi::ecs::INVALID_ENTITY;
        XMFLOAT3 point{};
        unsigned strikeIndex=0;
        float landedAt=-10.f;
        bool landed=false;
    };
    wi::ecs::Entity root=wi::ecs::INVALID_ENTITY;
    std::array<Slot,Visible> slots{};
    std::array<wi::ecs::Entity,CachedPairs> coreMeshes{}, auraMeshes{};
    wi::ecs::Entity groundMesh=wi::ecs::INVALID_ENTITY;
    unsigned cachedPairs=0;
    float seconds=0.f;
    unsigned contacts=0,evolvingFrames=0;
    static float Hash(unsigned v){
        v^=v>>16;v*=0x7feb352du;v^=v>>15;v*=0x846ca68bu;
        return float((v^(v>>16))&65535u)/65535.f;
    }
    static float Clamp(float v,float lo,float hi){return std::clamp(v,lo,hi);}
    static void Place(wi::scene::Scene& scene,wi::ecs::Entity e,const XMFLOAT3& p,float scale=1.f){
        if(auto* t=scene.transforms.GetComponent(e)){
            t->ClearTransform();t->Scale(XMFLOAT3{scale,scale,scale});
            t->Translate(p);t->UpdateTransform();
        }
    }
    static void Hide(wi::scene::Scene& scene,wi::ecs::Entity e){
        Place(scene,e,{0,0,0},.00001f);
    }
    static XMFLOAT3 Position(unsigned index,unsigned channel,float u,float time){
        const float seed=float(index)*1.713f;
        const float taper=std::pow(std::max(0.f,std::sin(XM_PI*u)),.65f);
        const float sx=(.205f*std::sin(12.1f*u+seed+time*10.f)
             +.145f*std::sin(20.9f*u+seed*.67f-time*14.f)
             +.062f*std::sin(39.1f*u-seed+time*23.f))*taper;
        const float sz=(.17f*std::cos(11.8f*u+seed*.81f-time*10.2f)
             +.087f*std::cos(29.3f*u-seed*.5f+time*13.5f))*taper;
        const float sep=channel==0?0.f:channel==1?.15f:channel==2?.20f:.105f;
        const float spin=XM_2PI*u*1.8f+time*10.8f+float(channel)*1.85f+seed;
        return {sx+sep*std::sin(spin)*(.45f+.55f*taper),
                5.56f*(1.f-u),
                sz+sep*std::cos(spin)*(.45f+.55f*taper)};
    }
    struct Geometry {
        std::vector<XMFLOAT3> v;
        std::vector<uint32_t> indices;
        void Tube(const std::vector<XMFLOAT3>& path,float radius){
            if(path.size()<2||radius<.001f)return;
            const auto base=uint32_t(v.size());
            for(size_t i=0;i<path.size();++i){
                const auto& p=path[i];
                const auto& prev=path[i?i-1:i];
                const auto& next=path[i+1<path.size()?i+1:i];
                auto d=XMVector3Normalize(XMLoadFloat3(&next)-XMLoadFloat3(&prev));
                const auto up=std::abs(XMVectorGetY(d))>.88f?
                    XMVectorSet(1,0,0,0):XMVectorSet(0,1,0,0);
                const auto x=XMVector3Normalize(XMVector3Cross(d,up));
                const auto z=XMVector3Normalize(XMVector3Cross(d,x));
                const float t=float(i)/float(path.size()-1);
                const float taper=.86f+.14f*std::sin(t*XM_PI);
                for(unsigned k=0;k<Sides;++k){
                    const float a=XM_2PI*float(k)/float(Sides);
                    const auto o=(x*std::cos(a)+z*std::sin(a))*radius*taper;
                    XMFLOAT3 out;XMStoreFloat3(&out,XMLoadFloat3(&p)+o);
                    v.push_back(out);
                }
            }
            for(unsigned i=0;i+1<path.size();++i)for(unsigned k=0;k<Sides;++k){
                const auto a=base+i*Sides+k;
                const auto b=base+i*Sides+(k+1)%Sides;
                const auto c=a+Sides,d=b+Sides;
                indices.insert(indices.end(),{a,b,c,b,d,c});
            }
            for(unsigned k=1;k+1<Sides;++k){
                indices.insert(indices.end(),{base,base+k+1,base+k});
                const unsigned end=base+unsigned(path.size()-1)*Sides;
                indices.insert(indices.end(),{end,end+k,end+k+1});
            }
        }
    };
    static Geometry Bolt(unsigned pose,unsigned stage,bool aura){
        Geometry result;
        const float time=float(pose)*.028f;
        const float reach=float(stage+1)/float(Stages);
        for(unsigned c=0;c<Strands;++c){
            const float r=c==0?.078f:c==1?.031f:c==2?.046f:.023f;
            std::vector<XMFLOAT3> path;
            path.reserve(Pieces+1);
            for(unsigned i=0;i<=Pieces;++i){
                const float u=reach*float(i)/Pieces;
                path.push_back(Position(0,c,u,time));
            }
            result.Tube(path,r*(aura?(c==0?2.5f:2.0f):1.f));
        }
        return result;
    }
    static Geometry Ground(){
        Geometry result;
        for(unsigned p=0;p<7;++p){
            const float angleBase=.28f+float(p/2)*2.0f+Hash(771u+p*11u)*.9f;
            float angle=angleBase;
            XMFLOAT3 now{(Hash(p*37u+50u)-.5f)*.4f,.04f,
                         (Hash(p*53u+43u)-.5f)*.4f};
            std::vector<XMFLOAT3> path{now};
            for(unsigned i=0;i<7;++i){
                angle+=(Hash(p*117u+i*13u+50u)-.5f)*1.1f;
                const float len=.105f+.125f*Hash(p*91u+i*43u+22u);
                now.x+=std::cos(angle)*len;
                now.z+=std::sin(angle)*len;
                now.y=.038f+.008f*Hash(p*17u+i*19u);
                path.push_back(now);
            }
            result.Tube(path,.012f+.01f*Hash(197u+p*113u));
        }
        return result;
    }
    wi::ecs::Entity Template(wi::scene::Scene& scene,Geometry&& g,
                             const char* name,const XMFLOAT4& base,const XMFLOAT4& emission){
        if(g.v.empty()||g.indices.empty())return wi::ecs::INVALID_ENTITY;
        const auto id=scene.Entity_CreateMeshFromData(name,g.indices.size(),g.indices.data(),
                                                       g.v.size(),g.v.data());
        if(auto* mesh=scene.meshes.GetComponent(id)){
            mesh->SetDoubleSided(true);
            mesh->SetQuantizedPositionsDisabled(true);
        }
        if(auto* material=scene.materials.GetComponent(id)){
            material->baseColor=base;material->emissiveColor=emission;
            material->userBlendMode=wi::enums::BLENDMODE_ADDITIVE;
            material->SetCastShadow(false);material->SetDoubleSided(true);
            material->SetDirty();
        }
        // Template mesh is an asset, not another rendered scene object.
        scene.objects.Remove(id);
        scene.Component_Attach(id,root,true);
        return id;
    }
    static wi::ecs::Entity Object(wi::scene::Scene& scene,wi::ecs::Entity root,const char* name){
        const auto id=scene.Entity_CreateObject(name);
        scene.Component_Attach(id,root,true);
        if(auto* o=scene.objects.GetComponent(id)){
            o->filterMask=wi::enums::FILTER_TRANSPARENT;o->SetCastShadow(false);
        }
        Hide(scene,id);
        return id;
    }
    bool Create(wi::scene::Scene& scene){
        if(root!=wi::ecs::INVALID_ENTITY)return true;
        root=scene.Entity_CreateTransform("Lightning R3 reusable spatial renderer");
        groundMesh=Template(scene,Ground(),"R3 ground current geometry",
             {.44f,.68f,1.f,.72f},{.55f,.79f,1.f,2.4f});
        for(auto& s:slots){
            s.core=Object(scene,root,"R3 connected white core");
            s.aura=Object(scene,root,"R3 connected blue aura");
            s.ground=Object(scene,root,"R3 ground crackling current");
            if(auto* obj=scene.objects.GetComponent(s.ground))obj->meshID=groundMesh;
            s.contact=scene.Entity_CreateSphere("R3 contact flash",1.f,10,10);
            scene.Component_Attach(s.contact,root,true);
            if(auto* m=scene.materials.GetComponent(s.contact)){
                m->baseColor={.94f,.98f,1.f,.84f};
                m->emissiveColor={.93f,.98f,1.f,5.f};
                m->userBlendMode=wi::enums::BLENDMODE_ADDITIVE;
                m->SetCastShadow(false);m->SetDirty();
            }
            if(auto* o=scene.objects.GetComponent(s.contact)){
                o->filterMask=wi::enums::FILTER_TRANSPARENT;o->SetCastShadow(false);
            }
            Hide(scene,s.contact);
            s.impactLight=scene.Entity_CreateLight("Lightning ground-contact dynamic light",{},
                XMFLOAT3{.61f,.8f,1.f},0.f,8.f,wi::scene::LightComponent::POINT);
            scene.Component_Attach(s.impactLight,root,true);
            s.skyLight=scene.Entity_CreateLight("Lightning channel dynamic light",{},
                XMFLOAT3{.57f,.75f,1.f},0.f,7.f,wi::scene::LightComponent::POINT);
            scene.Component_Attach(s.skyLight,root,true);
            s.sparks=bridge::CreateProjectileEffectEmitter(scene,
                {bridge::ProjectileEffectKind::Sparks,{},.065f,0.f,.3f},{});
            if(s.sparks==wi::ecs::INVALID_ENTITY)return false;
            scene.Component_Attach(s.sparks,root,true);
            if(auto* e=scene.emitters.GetComponent(s.sparks)){
                e->SetMaxParticleCount(260);e->life=.25f;e->random_life=.35f;
                e->velocity={0,6.1f,0};e->gravity={0,-9.8f,0};
                e->random_factor=1.1f;e->normal_factor=1.1f;
                e->scaleX=.65f;e->scaleY=1.6f;
                e->SetCollidersDisabled(false);e->count=0;
            }
            if(auto* m=scene.materials.GetComponent(s.sparks)){
                m->baseColor={.77f,.87f,1.f,.88f};
                m->emissiveColor={.62f,.81f,1.f,2.35f};m->SetDirty();
            }
            Hide(scene,s.sparks);
            if(auto* l=scene.lights.GetComponent(s.skyLight))l->intensity=0.f;
            if(auto* l=scene.lights.GetComponent(s.impactLight))l->intensity=0.f;
        }
        return true;
    }
    bool Ready()const {return cachedPairs==CachedPairs;}
    // Called incrementally while the equipment is idle, NOT on LMB.
    void Warmup(wi::scene::Scene& scene,unsigned budget=6){
        if(!Create(scene))return;
        for(unsigned n=0;n<budget && !Ready();++n){
            const unsigned pair=cachedPairs;
            const unsigned pose=pair/Stages,stage=pair%Stages;
            coreMeshes[pair]=Template(scene,Bolt(pose,stage,false),
                "R3 white continuous prebuilt pose",
                {.86f,.96f,1.f,.86f},{.93f,.98f,1.f,4.5f});
            auraMeshes[pair]=Template(scene,Bolt(pose,stage,true),
                "R3 blue continuous prebuilt pose",
                {.17f,.42f,.97f,.18f},{.25f,.52f,1.f,.56f});
            if(coreMeshes[pair]==wi::ecs::INVALID_ENTITY ||
               auraMeshes[pair]==wi::ecs::INVALID_ENTITY)return;
            ++cachedPairs;
        }
    }
    void Begin(wi::scene::Scene& scene){
        if(!Create(scene))return;
        // Never build meshes or upload texture atlases from casting.
        seconds=0.f;contacts=0;evolvingFrames=0;
        for(auto& s:slots){
            s.landed=false;s.landedAt=-10.f;
            Hide(scene,s.core);Hide(scene,s.aura);Hide(scene,s.ground);Hide(scene,s.contact);
            if(auto* e=scene.emitters.GetComponent(s.sparks))e->count=0.f;
            if(auto* l=scene.lights.GetComponent(s.skyLight))l->intensity=0.f;
            if(auto* l=scene.lights.GetComponent(s.impactLight))l->intensity=0.f;
        }
    }
    void SetFrame(wi::scene::Scene& scene,Slot& s,unsigned index,float fraction){
        if(!Ready())return;
        const unsigned pose=(unsigned(std::floor(seconds*55.f))+index*3u)%Poses;
        const unsigned stage=std::min(Stages-1,
            unsigned(Clamp(fraction,.001f,1.f)*float(Stages)-.001f));
        const unsigned key=pose*Stages+stage;
        if(auto* o=scene.objects.GetComponent(s.core))o->meshID=coreMeshes[key];
        if(auto* o=scene.objects.GetComponent(s.aura))o->meshID=auraMeshes[key];
        Place(scene,s.core,s.point);
        Place(scene,s.aura,s.point);
    }
    void Travel(wi::scene::Scene& scene,unsigned index,const XMFLOAT3& target,float fraction){
        if(root==wi::ecs::INVALID_ENTITY)return;
        auto& s=slots[index%Visible];
        s.point=target;s.strikeIndex=index;
        ++evolvingFrames;
        SetFrame(scene,s,index,fraction);
        Place(scene,s.skyLight,{target.x,target.y+2.3f,target.z});
        if(auto* l=scene.lights.GetComponent(s.skyLight))l->intensity=4.f+7.f*fraction;
    }
    void Land(wi::scene::Scene& scene,const XMFLOAT3& point,unsigned index){
        if(root==wi::ecs::INVALID_ENTITY)return;
        auto& s=slots[index%Visible];
        s.point=point;s.strikeIndex=index;s.landed=true;s.landedAt=seconds;
        ++contacts;
        SetFrame(scene,s,index,1.f);
        Place(scene,s.impactLight,{point.x,point.y+.3f,point.z});
        Place(scene,s.sparks,{point.x,point.y+.06f,point.z});
    }
    void Update(wi::scene::Scene& scene,float dt){
        if(root==wi::ecs::INVALID_ENTITY||!std::isfinite(dt)||dt<=0)return;
        seconds+=dt;
        for(auto& s:slots){
            if(!s.landed)continue;
            const float age=seconds-s.landedAt;
            const float flash=std::exp(-age*17.f);
            if(auto* l=scene.lights.GetComponent(s.impactLight))l->intensity=85.f*flash;
            if(auto* l=scene.lights.GetComponent(s.skyLight))l->intensity=12.f*flash;
            if(auto* e=scene.emitters.GetComponent(s.sparks))e->count=age<.07f?220.f:0.f;
            if(age<.10f){
                SetFrame(scene,s,s.strikeIndex,1.f);
                Place(scene,s.contact,{s.point.x,s.point.y+.06f,s.point.z},.10f+.08f*flash);
            }else {
                Hide(scene,s.core);Hide(scene,s.aura);Hide(scene,s.contact);
            }
            if(age>.05f&&age<.23f){
                // Irregular paths are one connected 3D mesh, not radial overlay cards.
                Place(scene,s.ground,s.point,Clamp((.23f-age)*6.f,.1f,1.f));
            }else Hide(scene,s.ground);
            if(age>.32f){
                s.landed=false;Hide(scene,s.ground);
                if(auto* l=scene.lights.GetComponent(s.impactLight))l->intensity=0;
                if(auto* l=scene.lights.GetComponent(s.skyLight))l->intensity=0;
                if(auto* e=scene.emitters.GetComponent(s.sparks))e->count=0;
            }
        }
    }
    void Reset(wi::scene::Scene& scene){
        if(root!=wi::ecs::INVALID_ENTITY)scene.Entity_Remove(root);
        *this={};
    }
};
} // namespace renegade::runtime
