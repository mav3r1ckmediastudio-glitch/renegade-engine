#pragma once
// Renegade-owned lunar authoring. Metadata is serialized; Weather GPU fields are transient.
#include "renegade/bridge/CommandService.h"
#include <WickedEngine.h>
#include <algorithm>
#include <cmath>

namespace renegade::bridge
{
    int MoonSurfaceDescriptor() noexcept;

    struct MoonState
    {
        bool enabled = false; // legacy scenes preserve their accepted sky
        bool automatic = true;
        float phase = 0.5f; // new=0/1, waxing quarter=.25, full=.5, waning quarter=.75
        float diameter = 1.0f; // angular diameter in degrees
        float brightness = 0.7f;
        float azimuth = 0.0f; // fixed heading, or automatic orbit heading offset
        float elevation = 35.0f; // fixed mode only
    };
    inline MoonState SanitizeMoon(MoonState s) noexcept
    {
        const auto safe = [](float v, float fallback, float lo, float hi)
        { return std::isfinite(v) ? std::clamp(v, lo, hi) : fallback; };
        s.phase = safe(s.phase,.5f,0,1);
        s.diameter = safe(s.diameter,1,.1f,12);
        s.brightness = safe(s.brightness,.7f,0,4);
        s.azimuth = safe(s.azimuth,0,-180,180);
        s.elevation = safe(s.elevation,35,-90,90);
        return s;
    }
    inline bool HasMoon(const wi::scene::Scene& scene, wi::ecs::Entity e) noexcept
    {
        const auto* m = scene.metadatas.GetComponent(e);
        return scene.weathers.Contains(e) && m && m->int_values.has("renegade.moon.schema") &&
            m->int_values.get("renegade.moon.schema") == 1;
    }
    inline MoonState CaptureMoon(const wi::scene::Scene& scene, wi::ecs::Entity e) noexcept
    {
        MoonState s;
        if (!HasMoon(scene,e)) return s;
        const auto& m = *scene.metadatas.GetComponent(e);
        if(m.int_values.has("renegade.moon.enabled")) s.enabled=m.int_values.get("renegade.moon.enabled")!=0;
        if(m.int_values.has("renegade.moon.automatic")) s.automatic=m.int_values.get("renegade.moon.automatic")!=0;
#define REN_MOON_READ(KEY,FIELD) if(m.float_values.has("renegade.moon." KEY)) s.FIELD=m.float_values.get("renegade.moon." KEY)
        REN_MOON_READ("phase",phase);
        REN_MOON_READ("diameter",diameter);
        REN_MOON_READ("brightness",brightness);
        REN_MOON_READ("azimuth",azimuth);
        REN_MOON_READ("elevation",elevation);
#undef REN_MOON_READ
        return SanitizeMoon(s);
    }
    inline void ApplyMoonRuntime(wi::scene::Scene& scene, wi::ecs::Entity e, MoonState s) noexcept
    {
        auto* w=scene.weathers.GetComponent(e);
        if(!w) return;
        s=SanitizeMoon(s);
        w->moon_parameters=XMFLOAT4(s.phase,s.diameter,s.enabled?s.brightness:0,
            s.enabled && s.brightness>0 ? float(MoonSurfaceDescriptor()) : -1.0f);
        w->moon_orbit=XMFLOAT4(s.azimuth,s.elevation,s.automatic?1.0f:0.0f,0);
        if(scene.weathers.GetCount() && scene.weathers.GetEntity(0)==e) scene.weather=*w;
    }
    inline bool StoreMoon(wi::scene::Scene& scene, wi::ecs::Entity e, MoonState s) noexcept
    {
        if(!scene.weathers.Contains(e)) return false;
        s=SanitizeMoon(s);
        auto* m=scene.metadatas.GetComponent(e);
        if(!m) m=&scene.metadatas.Create(e);
        m->int_values.set("renegade.moon.schema",1);
        m->int_values.set("renegade.moon.enabled",s.enabled?1:0);
        m->int_values.set("renegade.moon.automatic",s.automatic?1:0);
#define REN_MOON_WRITE(KEY,FIELD) m->float_values.set("renegade.moon." KEY,s.FIELD)
        REN_MOON_WRITE("phase",phase);
        REN_MOON_WRITE("diameter",diameter);
        REN_MOON_WRITE("brightness",brightness);
        REN_MOON_WRITE("azimuth",azimuth);
        REN_MOON_WRITE("elevation",elevation);
#undef REN_MOON_WRITE
        ApplyMoonRuntime(scene,e,s);
        return true;
    }
    inline void ClearMoon(wi::scene::Scene& scene, wi::ecs::Entity e) noexcept
    {
        auto* m=scene.metadatas.GetComponent(e);
        if(m)
        {
            for(const char* key:{"schema","enabled","automatic"})
                m->int_values.erase(std::string("renegade.moon.")+key);
            for(const char* key:{"phase","diameter","brightness","azimuth","elevation"})
                m->float_values.erase(std::string("renegade.moon.")+key);
        }
        ApplyMoonRuntime(scene,e,MoonState{});
    }
    inline void RestoreMoon(wi::scene::Scene& scene) noexcept
    {
        for(size_t i=0;i<scene.weathers.GetCount();++i)
        {
            const auto e=scene.weathers.GetEntity(i);
            ApplyMoonRuntime(scene,e,CaptureMoon(scene,e));
        }
    }
    inline bool SameMoon(MoonState a, MoonState b) noexcept
    {
        return a.enabled==b.enabled && a.automatic==b.automatic &&
            std::abs(a.phase-b.phase)<1e-5f && std::abs(a.diameter-b.diameter)<1e-5f &&
            std::abs(a.brightness-b.brightness)<1e-5f && std::abs(a.azimuth-b.azimuth)<1e-5f &&
            std::abs(a.elevation-b.elevation)<1e-5f;
    }
    // Shader uses the same artistic orbit: opposite the sun, with optional heading offset.
    inline XMFLOAT3 MoonDirection(MoonState s, XMFLOAT3 sun) noexcept
    {
        s=SanitizeMoon(s);
        const float a=XMConvertToRadians(s.azimuth);
        if(s.automatic)
        {
            const float len=std::sqrt(sun.x*sun.x+sun.y*sun.y+sun.z*sun.z);
            if(len<1e-6f || !std::isfinite(len)) sun=XMFLOAT3(0,1,0);
            else {sun.x/=len;sun.y/=len;sun.z/=len;}
            return XMFLOAT3(-sun.x*std::cos(a)-sun.z*std::sin(a),-sun.y,
                sun.x*std::sin(a)-sun.z*std::cos(a));
        }
        const float e=XMConvertToRadians(s.elevation);
        return XMFLOAT3(std::cos(e)*std::sin(a),std::sin(e),std::cos(e)*std::cos(a));
    }
    class SetMoonCommand final:public ICommand
    {
    public:
        SetMoonCommand(wi::scene::Scene& scene,wi::ecs::Entity e,MoonState next) noexcept
            :scene_(&scene),entity_(e),before_(CaptureMoon(scene,e)),
             after_(SanitizeMoon(next)),existed_(HasMoon(scene,e)) {}
        bool Execute() override
        { return !SameMoon(before_,after_) && StoreMoon(*scene_,entity_,after_); }
        void Undo() override
        { if(existed_) StoreMoon(*scene_,entity_,before_); else ClearMoon(*scene_,entity_); }
    private:
        wi::scene::Scene* scene_;
        wi::ecs::Entity entity_;
        MoonState before_,after_;
        bool existed_;
    };
}
