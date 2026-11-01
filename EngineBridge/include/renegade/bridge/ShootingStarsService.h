#pragma once
#include "renegade/bridge/CommandService.h"
#include <WickedEngine.h>
#include <algorithm>
#include <cmath>

namespace renegade::bridge
{
    struct ShootingStarsState
    {
        bool enabled=false;
        float frequency=2.0f; // average sightings per minute, across the whole sky
        float brightness=1.0f;
        float trailLength=8.0f; // angular degrees
    };
    inline ShootingStarsState SanitizeShootingStars(ShootingStarsState s) noexcept
    {
        const auto safe=[](float v,float fallback,float lo,float hi)
        {return std::isfinite(v)?std::clamp(v,lo,hi):fallback;};
        s.frequency=safe(s.frequency,2,.1f,120);
        s.brightness=safe(s.brightness,1,0,4);
        s.trailLength=safe(s.trailLength,8,1,25);
        return s;
    }
    inline bool HasShootingStars(const wi::scene::Scene& scene,wi::ecs::Entity e) noexcept
    {
        const auto* m=scene.metadatas.GetComponent(e);
        return scene.weathers.Contains(e) && m && m->int_values.has("renegade.meteors.schema") &&
            m->int_values.get("renegade.meteors.schema")==1;
    }
    inline ShootingStarsState CaptureShootingStars(const wi::scene::Scene& scene,wi::ecs::Entity e) noexcept
    {
        ShootingStarsState s;
        if(!HasShootingStars(scene,e)) return s;
        const auto& m=*scene.metadatas.GetComponent(e);
        if(m.int_values.has("renegade.meteors.enabled"))
            s.enabled=m.int_values.get("renegade.meteors.enabled")!=0;
#define REN_METEOR_READ(KEY,FIELD) if(m.float_values.has("renegade.meteors." KEY)) s.FIELD=m.float_values.get("renegade.meteors." KEY)
        REN_METEOR_READ("frequency",frequency);
        REN_METEOR_READ("brightness",brightness);
        REN_METEOR_READ("trail",trailLength);
#undef REN_METEOR_READ
        return SanitizeShootingStars(s);
    }
    inline void ApplyShootingStarsRuntime(wi::scene::Scene& scene,wi::ecs::Entity e,ShootingStarsState s) noexcept
    {
        auto* w=scene.weathers.GetComponent(e);
        if(!w) return;
        s=SanitizeShootingStars(s);
        w->shooting_stars=XMFLOAT3(s.enabled?s.frequency:0,s.brightness,s.trailLength);
        if(!s.enabled || s.brightness<=0) w->shooting_star_preview_time=-1000;
        if(scene.weathers.GetCount() && scene.weathers.GetEntity(0)==e) scene.weather=*w;
    }
    inline bool StoreShootingStars(wi::scene::Scene& scene,wi::ecs::Entity e,ShootingStarsState s) noexcept
    {
        if(!scene.weathers.Contains(e)) return false;
        s=SanitizeShootingStars(s);
        auto* m=scene.metadatas.GetComponent(e);if(!m)m=&scene.metadatas.Create(e);
        m->int_values.set("renegade.meteors.schema",1);
        m->int_values.set("renegade.meteors.enabled",s.enabled?1:0);
        m->float_values.set("renegade.meteors.frequency",s.frequency);
        m->float_values.set("renegade.meteors.brightness",s.brightness);
        m->float_values.set("renegade.meteors.trail",s.trailLength);
        ApplyShootingStarsRuntime(scene,e,s);return true;
    }
    inline void ClearShootingStars(wi::scene::Scene& scene,wi::ecs::Entity e) noexcept
    {
        auto* m=scene.metadatas.GetComponent(e);
        if(m)
        {
            m->int_values.erase("renegade.meteors.schema");
            m->int_values.erase("renegade.meteors.enabled");
            for(const char* k:{"frequency","brightness","trail"})
                m->float_values.erase(std::string("renegade.meteors.")+k);
        }
        ApplyShootingStarsRuntime(scene,e,{});
    }
    inline void RestoreShootingStars(wi::scene::Scene& scene) noexcept
    {
        for(std::size_t i=0;i<scene.weathers.GetCount();++i)
        {const auto e=scene.weathers.GetEntity(i);ApplyShootingStarsRuntime(scene,e,CaptureShootingStars(scene,e));}
    }
    inline bool SameShootingStars(ShootingStarsState a,ShootingStarsState b) noexcept
    {
        return a.enabled==b.enabled && std::abs(a.frequency-b.frequency)<1e-5f &&
            std::abs(a.brightness-b.brightness)<1e-5f && std::abs(a.trailLength-b.trailLength)<1e-5f;
    }
    // Explicit preview is transient: it does not alter the scene's metadata/history.
    inline void PreviewShootingStar(wi::scene::Scene& scene,wi::ecs::Entity e,float rendererTime) noexcept
    {
        auto* w=scene.weathers.GetComponent(e);
        if(!w || !std::isfinite(rendererTime) || rendererTime<0 || w->shooting_stars.x<=0) return;
        w->shooting_star_preview_time=rendererTime;
        if(scene.weathers.GetCount() && scene.weathers.GetEntity(0)==e) scene.weather=*w;
    }
    class SetShootingStarsCommand final:public ICommand
    {
    public:
        SetShootingStarsCommand(wi::scene::Scene& scene,wi::ecs::Entity e,ShootingStarsState next) noexcept
            :scene_(&scene),e_(e),before_(CaptureShootingStars(scene,e)),after_(SanitizeShootingStars(next)),
             existed_(HasShootingStars(scene,e)){}
        bool Execute() override
        {return !SameShootingStars(before_,after_) && StoreShootingStars(*scene_,e_,after_);}
        void Undo() override
        {if(existed_)StoreShootingStars(*scene_,e_,before_);else ClearShootingStars(*scene_,e_);}
    private:
        wi::scene::Scene* scene_;
        wi::ecs::Entity e_;
        ShootingStarsState before_,after_;
        bool existed_;
    };
}
