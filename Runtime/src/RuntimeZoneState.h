#pragma once
#include "renegade/bridge/ZoneService.h"
#include "renegade/bridge/WeatherTransitionService.h"
#include <algorithm>
#include <optional>
#include <unordered_map>

namespace renegade::runtime
{
    struct ZoneContact
    {
        wi::ecs::Entity id;
        bool inside=false, won=false, scripted=false, active=false, fired=false, suppressArrival=false;
        float cooldown=0;
        bool healthRunning=false;
    };

    // Exactly one exit for each accepted activation, including interaction and cooldown.
    enum class ZoneEdge { None, Enter, Exit };
    inline ZoneEdge SampleZone(ZoneContact& c, const bridge::zones::Definition& z,
                               bool inside, bool interact, float dt)
    {
        c.cooldown=std::max(0.f,c.cooldown-dt);
        const bool crossing=inside && !c.inside;
        c.inside=inside;
        if (!inside)
        {
            c.suppressArrival=false;
            if (!c.active) return ZoneEdge::None;
            c.active=false;
            return ZoneEdge::Exit;
        }
        if (c.suppressArrival || c.active || (z.repeat==1 && c.fired) || c.cooldown>0) return ZoneEdge::None;
        if (!(z.activation==1 ? interact : crossing)) return ZoneEdge::None;
        c.fired=c.active=true;
        if (z.repeat==2) c.cooldown=z.cooldown;
        return ZoneEdge::Enter;
    }

    struct ZonePose { XMFLOAT3 position{}; float yaw=0; };
    struct ZoneMessage { wi::ecs::Entity owner; std::string text; float remaining; };
    struct ZoneWeather { wi::ecs::Entity owner; int preset; };
    struct ZoneCue { wi::ecs::Entity owner, source; };
    struct ZoneFade
    {
        float volume=1, from=0, to=0, elapsed=0, duration=0;
        bool originallyPlaying=false, release=false;
    };
    struct ZoneEffects
    {
        std::optional<ZonePose> checkpoint;
        bridge::EntityIdentityIndex identities;
        std::vector<ZoneMessage> messages;
        std::unordered_map<std::string,int> objectives; // -1 completed, otherwise completed steps
        std::vector<ZoneWeather> weather;
        std::optional<bridge::WeatherState> outsideWeather;
        bridge::WeatherTransitionController weatherBlend;
        std::vector<ZoneCue> cues;
        std::unordered_map<wi::ecs::Entity,ZoneFade> fades;
    };
}
