#pragma once
#include "RuntimeChannelVisual.h"
#include "RuntimeLightningStage3.h"
#include "RuntimeLightningNatural.h"
#include "RuntimeProjectileWorld.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace renegade::runtime {

// Presentation-only lightning atop the governed P8 timed AoE scheduler.
// A single centre-out travelling electrical discharge with event-authoritative
// impacts. Cosmetic Wicked scene entities never determine hit or damage.
struct RuntimeLightningStorm {
    static constexpr unsigned MaxStrikes=64,Slots=3;
    static constexpr unsigned RingSides=72,RingTicks=8;
    struct Strike {
        XMFLOAT3 position{};
        float remaining=0;
        wi::ecs::Entity impact=wi::ecs::INVALID_ENTITY;
        wi::ecs::Entity hotHalo=wi::ecs::INVALID_ENTITY;
        std::array<wi::ecs::Entity,7> travellingArcs{};
        wi::ecs::Entity travellingHead=wi::ecs::INVALID_ENTITY;
        XMFLOAT3 target{};
        float travelStart=0,travelDuration=.15f;
        unsigned travelIndex=0;
        bool travelling=false;
        wi::ecs::Entity sparks=wi::ecs::INVALID_ENTITY;
    };
    wi::ecs::Entity root=wi::ecs::INVALID_ENTITY;
    bool stage3Enabled=false;
    RuntimeLightningStage3 stage3;
    bool naturalEnabled=false;
    NaturalStorm natural;
    std::array<wi::ecs::Entity,RingSides> outerRing{},innerRing{};
    std::array<wi::ecs::Entity,RingTicks> ringTicks{};
    std::array<Strike,Slots> visible{};
    wi::ecs::Entity light=wi::ecs::INVALID_ENTITY;
    wi::ecs::Entity blueLight=wi::ecs::INVALID_ENTITY;
    wi::ecs::Entity centralSeed=wi::ecs::INVALID_ENTITY;
    bridge::ProjectileSource source;
    XMFLOAT3 center{};
    float radius=2.5f,duration=2.6f,strikeDamage=21.f,elapsed=0;
    unsigned strikeCount=8,fired=0,landed=0;
    bool active=false,preview=false;
    static float Hash(unsigned seed) noexcept {
        unsigned n=seed*747796405u+2891336453u;
        n=((n>>((n>>28)+4))^n)*277803737u;
        n=(n>>22)^n;
        return float(n&0xffffu)/65535.f;
    }
    static XMFLOAT3 DiscPosition(const XMFLOAT3& at,float r,unsigned i,unsigned count) noexcept {
        // V11: deterministic radial propagation, never random rearrangement.
        // Strike zero hits the centre; subsequent impacts occupy increasingly
        // distant shells, fanning out like a small electrical shotgun blast.
        if(i==0 || count<=1)return at;
        const float progress=float(i)/float(count-1);
        const float span=r*.95f*std::pow(progress,.82f);
        const float angle=float(i)*2.39996323f+(Hash(i+207u)-.5f)*.38f;
        return {at.x+span*std::cos(angle),at.y,at.z+span*std::sin(angle)};
    }
    static float LaunchTime(unsigned index,unsigned count,float lifetime) noexcept {
        if(count<=1)return std::min(.08f,lifetime*.12f);
        // The first 60% of the spell sends the wave outward, with enough
        // time for visible travelling leaders; the remaining time is the
        // residual atmospheric fading tail.
        return lifetime*(.035f+.56f*float(index)/float(count-1));
    }
    static float TravelTime(float radialDistance,float radius) noexcept {
        // Keep the electric fan in sight for 20-40 rendered frames, not a
        // 5-frame streak that looks like the previous instantaneous strike.
        return .28f+.23f*std::clamp(radialDistance/std::max(.1f,radius),0.f,1.f);
    }
    static bool Within(float x,float z,const XMFLOAT3& at,float r) noexcept {
        const float dx=x-at.x,dz=z-at.z;
        return std::isfinite(dx)&&std::isfinite(dz)&&dx*dx+dz*dz<=r*r;
    }
    void Reset(wi::scene::Scene& scene) {
        stage3.Reset(scene);
        natural.Reset(scene);
        if(root!=wi::ecs::INVALID_ENTITY)scene.Entity_Remove(root);
        *this={};
    }
    void UseStage3(wi::scene::Scene& scene,bool enabled){
        if(stage3Enabled==enabled)return;
        if(!active)Reset(scene);
        stage3Enabled=enabled;
    }

    void UseNatural(wi::scene::Scene& scene,bool enabled){
        if(naturalEnabled==enabled)return;
        if(!active)Reset(scene);
        naturalEnabled=enabled;
    }

    wi::ecs::Entity MakeReliableRingSegment(wi::scene::Scene& scene,const char* name,
                                            XMFLOAT4 color,XMFLOAT4 glow){
        // Built-in cube has flat ends and a constant width when projected
        // along the segment. Sphere segments made the V8/V9 ring bead up.
        const auto id=scene.Entity_CreateCube(name);
        scene.Component_Attach(id,root,true);
        if(auto* m=scene.materials.GetComponent(id)){
            m->baseColor=color;m->emissiveColor=glow;
            m->userBlendMode=wi::enums::BLENDMODE_ALPHA;
            m->SetCastShadow(false);m->SetDirty();
        }
        if(auto* obj=scene.objects.GetComponent(id))
            obj->filterMask=wi::enums::FILTER_TRANSPARENT;
        return id;
    }
    void MakeReliableRing(wi::scene::Scene& scene){
        for(auto& e:outerRing)e=MakeReliableRingSegment(scene,"Storm deep blue perimeter",
            {.04f,.28f,.78f,.82f},{.03f,.26f,.65f,.6f});
        for(auto& e:innerRing)e=MakeReliableRingSegment(scene,"Storm inner azure guide",
            {.12f,.45f,.9f,.51f},{.07f,.4f,.8f,.35f});
        for(auto& e:ringTicks)e=MakeReliableRingSegment(scene,"Storm fine tick",
            {.2f,.57f,.98f,.62f},{.12f,.44f,.85f,.45f});
    }
    void Ensure(wi::scene::Scene& scene){
        if(root!=wi::ecs::INVALID_ENTITY)return;
        root=scene.Entity_CreateTransform("Lightning Storm VFX (not collision)");
        // Natural visuals own a compact renderer, not the legacy preview/bolt objects.
        if(naturalEnabled)return;
        // Use the verified native flat-segment marker; no legacy V9 mesh.
        MakeReliableRing(scene);
        for(auto& s:visible){
            s.impact=scene.Entity_CreateSphere("Storm white-hot contact core");
            scene.Component_Attach(s.impact,root,true);
            if(auto* m=scene.materials.GetComponent(s.impact)){
                // A fast contact flash, not a permanently glowing cartoon orb.
                m->baseColor={.91f,.96f,1.f,.24f};
                m->emissiveColor={.69f,.85f,1.f,3.1f};
                m->userBlendMode=wi::enums::BLENDMODE_ADDITIVE;m->SetDirty();
            }
            if(auto* o=scene.objects.GetComponent(s.impact))
                o->filterMask=wi::enums::FILTER_TRANSPARENT;
            RuntimeChannelVisual::Place(scene,s.impact,XMVectorZero(),.00001f);
            s.hotHalo=scene.Entity_CreateSphere("Storm diffused blue ground flash");
            scene.Component_Attach(s.hotHalo,root,true);
            if(auto* m=scene.materials.GetComponent(s.hotHalo)){
                m->baseColor={.49f,.69f,1.f,.16f};
                m->emissiveColor={.36f,.59f,1.f,1.9f};
                m->userBlendMode=wi::enums::BLENDMODE_ADDITIVE;
                m->SetCastShadow(false);m->SetDirty();
            }
            if(auto* o=scene.objects.GetComponent(s.hotHalo))
                o->filterMask=wi::enums::FILTER_TRANSPARENT;
            RuntimeChannelVisual::Place(scene,s.hotHalo,XMVectorZero(),.00001f);
            // Ground energy is native particle sparks and a brief halo,
            // not rigid magenta/orange spikes radiating from a plastic sphere.
            // The only strike geometry is this single expanding leader.
            // Its endpoint is also the origin of the contact flash.
            for(unsigned j=0;j<s.travellingArcs.size();++j){
                // One connected irregular discharge. Hairline bright trunk
                // with thinner electric forks; not additional solid blue
                // pipes drawn over an unrelated legacy vertical bolt.
                const bool core=j<5;
                s.travellingArcs[j]=MakeReliableRingSegment(scene,"Storm single expanding discharge",
                    core?XMFLOAT4{.75f,.9f,1.f,.77f}:XMFLOAT4{.24f,.54f,1.f,.47f},
                    core?XMFLOAT4{.68f,.82f,1.f,2.5f}:XMFLOAT4{.19f,.42f,.86f,1.15f});
                RuntimeChannelVisual::Place(scene,s.travellingArcs[j],XMVectorZero(),.00001f);
            }
            s.travellingHead=scene.Entity_CreateSphere("Storm travelling charged front");
            scene.Component_Attach(s.travellingHead,root,true);
            if(auto* m=scene.materials.GetComponent(s.travellingHead)){
                m->baseColor={.73f,.89f,1.f,.18f};
                m->emissiveColor={.35f,.67f,1.f,1.2f};
                m->userBlendMode=wi::enums::BLENDMODE_ADDITIVE;
                m->SetCastShadow(false);m->SetDirty();
            }
            if(auto* o=scene.objects.GetComponent(s.travellingHead))
                o->filterMask=wi::enums::FILTER_TRANSPARENT;
            RuntimeChannelVisual::Place(scene,s.travellingHead,XMVectorZero(),.00001f);
            s.sparks=bridge::CreateProjectileEffectEmitter(scene,
                {bridge::ProjectileEffectKind::Sparks,{},.028f,0.f,.3f},{});
            scene.Component_Attach(s.sparks,root,true);
            if(auto* m=scene.materials.GetComponent(s.sparks)){
                m->baseColor={.71f,.85f,1.f,.65f};
                m->emissiveColor={.51f,.71f,1.f,1.6f};m->SetDirty();
            }
            if(auto* e=scene.emitters.GetComponent(s.sparks)){
                e->life=.24f;e->random_life=.45f;e->gravity={0,-9.8f,0};
                e->velocity={0,2.3f,0};e->random_factor=.75f;
                e->normal_factor=.68f;e->SetCollidersDisabled(false);
                e->SetMaxParticleCount(140);
            }
        }
        light=scene.Entity_CreateLight("Storm contact illumination",{},
            XMFLOAT3{.66f,.82f,1.f},1.f,4.2f,wi::scene::LightComponent::POINT);
        scene.Component_Attach(light,root,true);
        blueLight=scene.Entity_CreateLight("Storm ambient blue charge",{},
            XMFLOAT3{.12f,.43f,1.f},.0f,8.5f,wi::scene::LightComponent::POINT);
        scene.Component_Attach(blueLight,root,true);
        centralSeed=scene.Entity_CreateSphere("Storm central azure seed");
        scene.Component_Attach(centralSeed,root,true);
        if(auto* m=scene.materials.GetComponent(centralSeed)){
            m->baseColor={.28f,.57f,1.f,.12f};
            m->emissiveColor={.17f,.48f,.84f,.9f};
            m->userBlendMode=wi::enums::BLENDMODE_ADDITIVE;
            m->SetCastShadow(false);m->SetDirty();
        }
        if(auto* obj=scene.objects.GetComponent(centralSeed))
            obj->filterMask=wi::enums::FILTER_TRANSPARENT;
        RuntimeChannelVisual::Place(scene,centralSeed,XMVectorZero(),.00001f);
    }
    void SetMarker(wi::scene::Scene& scene,const XMFLOAT3& point,float r,bool visible) {
        Ensure(scene);center=point;radius=r;preview=visible;
        if(naturalEnabled)return;
        if(stage3Enabled)stage3.Preview(scene,point,r,visible);
        const float y=point.y+.08f;
        const auto arcPoint=[&](float angle,float scale){
            return XMVectorSet(point.x+std::cos(angle)*r*scale,y,
                point.z+std::sin(angle)*r*scale,1);
        };
        for(unsigned i=0;i<RingSides;++i){
            const float a=XM_2PI*float(i)/float(RingSides);
            const float b=XM_2PI*float(i+1)/float(RingSides);
            RuntimeChannelVisual::Segment(scene,outerRing[i],arcPoint(a,1),arcPoint(b,1),
                visible&&!stage3Enabled&&!naturalEnabled?.028f:.00001f);
            RuntimeChannelVisual::Segment(scene,innerRing[i],arcPoint(a,.78f),arcPoint(b,.78f),
                visible&&!stage3Enabled&&!naturalEnabled?.011f:.00001f);
        }
        for(unsigned i=0;i<RingTicks;++i){
            const float a=XM_2PI*float(i)/float(RingTicks);
            RuntimeChannelVisual::Segment(scene,ringTicks[i],arcPoint(a,.85f),arcPoint(a,.975f),
                visible&&!stage3Enabled&&!naturalEnabled?.019f:.00001f);
        }
    }
    void Begin(wi::scene::Scene& scene,bridge::ProjectileSource caster,const XMFLOAT3& at,
               float r,float time,unsigned count,float damage){
        const bool useNewVisual=stage3Enabled;
        const bool useNatural=naturalEnabled;
        if(useNatural){
            // Preserve meshes prepared during idle. Casting must never regenerate them.
            for(auto& slot:visible){slot.remaining=0.f;slot.travelling=false;}
            fired=0;landed=0;elapsed=0.f;active=false;
        }else Reset(scene);
        stage3Enabled=useNewVisual;naturalEnabled=useNatural;
        source=std::move(caster);radius=r;duration=time;
        strikeCount=std::min(count,MaxStrikes);strikeDamage=damage;active=true;
        SetMarker(scene,at,r,!stage3Enabled&&!naturalEnabled);
        if(stage3Enabled)stage3.Begin(scene,at,r,strikeCount);
        if(naturalEnabled)natural.BeginLive(scene);
        if(auto* t=scene.transforms.GetComponent(blueLight)){
            t->ClearTransform();
            t->Translate(XMFLOAT3{at.x,at.y+.55f,at.z});
            t->UpdateTransform();
        }
    }
    void HideTravel(wi::scene::Scene& scene,Strike& s) {
        for(auto e:s.travellingArcs)
            RuntimeChannelVisual::Place(scene,e,XMVectorZero(),.00001f);
        RuntimeChannelVisual::Place(scene,s.travellingHead,XMVectorZero(),.00001f);
    }
    void StartTravel(wi::scene::Scene& scene,unsigned index,float launchAt) {
        auto& s=visible[index%Slots];
        s.target=DiscPosition(center,radius,index,strikeCount);
        s.travelIndex=index;
        s.travelStart=launchAt;
        const float dx=s.target.x-center.x,dz=s.target.z-center.z;
        s.travelDuration=TravelTime(std::hypot(dx,dz),radius);
        s.travelling=true;
        HideTravel(scene,s);
    }
    void AnimateTravel(wi::scene::Scene& scene,Strike& s,float progress) {
        if(stage3Enabled){stage3.Travel(scene,s.travelIndex,s.target,progress);return;}
        if(naturalEnabled){natural.Travel(scene,s.travelIndex,s.target,progress);return;}
        // The first strike descends to the central impact; subsequent
        // branches originate there and travel radially across the floor.
        // No secondary vertical bolt and no unnatural overhead "tent".
        const float t=1.f-std::pow(1.f-std::clamp(progress,0.f,1.f),1.2f);
        const float dx=s.target.x-center.x,dz=s.target.z-center.z;
        const float dist=std::hypot(dx,dz);
        const float angle=float(s.travelIndex)*2.39996323f;
        const float nx=dist>.03f?-dz/dist:std::cos(angle);
        const float nz=dist>.03f?dx/dist:std::sin(angle);
        // The first discharge descends into the centre. Every later
        // discharge propagates FROM that impact point at ground level.
        // Never create eight suspended spokes from a floating common hub.
        const bool mainStrike=s.travelIndex==0;
        const float height=mainStrike?5.4f:.22f;
        if(t<.015f){HideTravel(scene,s);return;}
        std::array<XMVECTOR,6> knots{};
        const float seconds=elapsed-s.travelStart;
        for(unsigned i=0;i<knots.size();++i){
            const float u=float(i)/float(knots.size()-1);
            const float envelope=std::sin(u*XM_PI);
            const float irregular=(std::sin(u*37.f+seconds*31.f+angle)*.073f+
                                   std::sin(u*91.f-seconds*20.f+angle)*.043f)*envelope;
            knots[i]=XMVectorSet(center.x+dx*t*u+nx*irregular,
                center.y+height*(1.f-t*u)+.07f*t*u+
                    (mainStrike?.035f:.11f)*std::sin(u*51.f+seconds*13.f)*envelope,
                center.z+dz*t*u+nz*irregular,1);
        }
        for(unsigned i=0;i<5;++i){
            const float taper=1.f-.16f*float(i)/5.f;
            RuntimeChannelVisual::Segment(scene,s.travellingArcs[i],
                knots[i],knots[i+1],(.020f+(i==0?.004f:0.f))*taper);
        }
        const auto fork=[&](unsigned from,unsigned branch,float sign){
            const float lateral=sign*(.17f+.13f*t);
            const XMVECTOR tip=knots[from]+XMVectorSet(nx*lateral,.11f*sign,nz*lateral,0);
            RuntimeChannelVisual::Segment(scene,s.travellingArcs[branch],
                knots[from],tip,.010f*(1.f-.26f*t));
        };
        fork(2,5,1.f);
        fork(4,6,-1.f);
        RuntimeChannelVisual::Place(scene,s.travellingHead,knots.back(),.034f);
    }
    template<class Emit>
    void Tick(wi::scene::Scene& scene,float dt,Emit onStrike){
        if(!std::isfinite(dt)||dt<=0||root==wi::ecs::INVALID_ENTITY)return;
        if(stage3Enabled)stage3.Tick(scene,dt);
        if(naturalEnabled)natural.TickLive(scene,dt);
        for(auto& s:visible){
            s.remaining=std::max(0.f,s.remaining-dt);
            if(s.remaining<=0.f){
                if(!s.travelling)HideTravel(scene,s);
                if(stage3Enabled&&!s.travelling)stage3.EndArc(scene,s.travelIndex);
                if(auto* emit=scene.emitters.GetComponent(s.sparks))emit->count=0;
                RuntimeChannelVisual::Place(scene,s.impact,XMVectorZero(),.00001f);
                RuntimeChannelVisual::Place(scene,s.hotHalo,XMVectorZero(),.00001f);
            }else if(!stage3Enabled&&!naturalEnabled){
                // The *same travelling bolt* terminates in one short contact
                // flash; no replacement rod/bolt suddenly appears on arrival.
                const float f=std::clamp(s.remaining/.14f,0.f,1.f);
                RuntimeChannelVisual::Place(scene,s.impact,
                    XMVectorSet(s.position.x,s.position.y+.027f,s.position.z,1),
                    .30f*std::sqrt(f));
                if(auto* t=scene.transforms.GetComponent(s.hotHalo)){
                    t->ClearTransform();
                    t->Scale(XMFLOAT3{.64f*std::sqrt(f),.023f,.64f*std::sqrt(f)});
                    t->Translate(XMFLOAT3{s.position.x,s.position.y+.035f,s.position.z});
                    t->UpdateTransform();
                }
                if(s.remaining<.04f)HideTravel(scene,s);
            }
        }
        if(light!=wi::ecs::INVALID_ENTITY)
            if(auto* l=scene.lights.GetComponent(light)){
                float peak=0.f;
                for(const auto& s:visible)peak=std::max(peak,s.remaining);
                l->intensity=8.f*std::clamp(peak/.14f,0.f,1.f);
            }
        if(!active){
            if(auto* l=scene.lights.GetComponent(blueLight))l->intensity=0.f;
            RuntimeChannelVisual::Place(scene,centralSeed,XMVectorZero(),.00001f);
            return;
        }
        elapsed+=dt;
        if(auto* l=scene.lights.GetComponent(blueLight))
            l->intensity=(stage3Enabled||naturalEnabled)?0.f:2.5f+.7f*std::sin(elapsed*16.f)*std::sin(elapsed*16.f);
        const float seedFalloff=std::clamp((duration-elapsed)/.9f,0.f,1.f);
        RuntimeChannelVisual::Place(scene,centralSeed,
            XMVectorSet(center.x,center.y+.095f,center.z,1),
            (stage3Enabled||naturalEnabled)?.00001f:(.025f+.018f*seedFalloff));
        // Launch a deterministic centre-out shotgun-like electrical wave.
        // Reuse bounded effect slots: under unusually high user-configured
        // strike counts a busy slot queues its next launch, never overwrites
        // another live hit or drops a scheduled damage event.
        for(;fired<strikeCount;++fired) {
            const float scheduled=LaunchTime(fired,strikeCount,duration);
            if(scheduled>elapsed)break;
            auto& slot=visible[fired%Slots];
            if(slot.travelling || slot.remaining>0.f)break;
            StartTravel(scene,fired,scheduled);
        }
        for(auto& s:visible) {
            if(!s.travelling)continue;
            const float progress=std::clamp(
                (elapsed-s.travelStart)/std::max(.03f,s.travelDuration),0.f,1.f);
            if(progress<1.f){
                AnimateTravel(scene,s,progress);
                continue;
            }
            // The moving electrical front has NOW reached the target.
            // Gameplay is resolved here, exactly once; no visual overlap or
            // per-frame proximity damage can trigger extra events.
            // Keep the final frame of the SAME arriving bolt lit briefly
            // through contact; never hand off to an unrelated static bolt.
            AnimateTravel(scene,s,1.f);
            s.travelling=false;
            auto hit=s.target;
            if(landed==0)SetMarker(scene,center,radius,false);
            onStrike(hit,s.travelIndex);
            SpawnStrike(scene,hit,s.travelIndex);
            ++landed;
        }
        if(elapsed>=duration && landed==strikeCount)active=false;
    }
    void SpawnStrike(wi::scene::Scene& scene,const XMFLOAT3& at,unsigned id){
        if(naturalEnabled){
            auto& s=visible[id%Slots];s.position=at;s.remaining=.14f;
            natural.Land(scene,at,id);
            return;
        }
        if(stage3Enabled){
            auto& s=visible[id%Slots];s.position=at;s.remaining=.14f;
            stage3.Land(scene,at,id);
            return;
        }
        // The physical discharge just arrived here: one contact burst,
        // deliberately NO second static 8-metre lightning stroke.
        auto& s=visible[id%Slots];s.position=at;s.remaining=.14f;
        RuntimeChannelVisual::Place(scene,s.impact,XMLoadFloat3(&at),.19f);
        if(auto* t=scene.transforms.GetComponent(s.sparks)){
            t->ClearTransform();t->Translate(at);t->UpdateTransform();
        }
        if(auto* e=scene.emitters.GetComponent(s.sparks))e->count=95.f;
        if(auto* t=scene.transforms.GetComponent(light)){
            t->ClearTransform();
            t->Translate(XMFLOAT3{at.x,at.y+.9f,at.z});
            t->UpdateTransform();
        }
        if(auto* l=scene.lights.GetComponent(light))l->intensity=8.f;
    }
};
}
