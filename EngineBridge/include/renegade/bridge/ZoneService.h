#pragma once

#include "renegade/bridge/CommandService.h"
#include "renegade/bridge/IdentityService.h"
#include <array>
#include <cmath>

namespace renegade::bridge::zones
{
    using Entity = wi::ecs::Entity;
    using Scene = wi::scene::Scene;
    enum class Behaviour { Trigger, Win, Message, Objective, Checkpoint, Transition, Teleport, Health, Kill, Encounter, Dialogue, Ambience, Environment, Count };
    struct Preset
    {
        const char *label, *family, *source, *onEnter, *onExit, *targetLabel;
        const char *primaryLabel, *secondaryLabel, *primaryDefault, *secondaryDefault;
        const char *modes, *amountLabel, *secondsLabel;
        float amount, seconds;
    };
    inline constexpr std::array<Preset, size_t(Behaviour::Count)> Presets{{
        {"Custom Trigger", "Trigger", "Content/Scripts/Library/renegade.zones.trigger/TriggerZone.lua", "zone.trigger.enter", "zone.trigger.exit", "Event target", "Enter event", "Exit event", "enter", "exit", "", "", "", 0, 0},
        {"Win Zone", "Win", "Content/Scripts/Library/renegade.zones.win/WinZone.lua", "zone.win.enter", "zone.win.exit", "", "", "", "", "", "", "", "", 0, 0},
        {"Message / Tutorial", "Message", "Content/Scripts/Library/renegade.zones.message/MessageZone.lua", "zone.message.enter", "zone.message.exit", "", "Message", "", "Welcome!", "", "Timed|While inside", "", "Duration (s)", 0, 4},
        {"Objective", "Objective", "Content/Scripts/Library/renegade.zones.objective/ObjectiveZone.lua", "zone.objective.enter", "zone.objective.exit", "", "Objective ID", "", "first.objective", "", "Start|Advance|Complete", "", "", 0, 0},
        {"Checkpoint", "Checkpoint", "Content/Scripts/Library/renegade.zones.checkpoint/CheckpointZone.lua", "zone.checkpoint.enter", "zone.checkpoint.exit", "Respawn marker", "", "", "", "", "", "", "", 0, 0},
        {"Level Transition", "Transition", "Content/Scripts/Library/renegade.zones.transition/TransitionZone.lua", "zone.transition.enter", "zone.transition.exit", "", "Flow outcome", "Arrival marker", "level.complete", "", "", "", "", 0, 0},
        {"Teleport", "Teleport", "Content/Scripts/Library/renegade.zones.teleport/TeleportZone.lua", "zone.teleport.enter", "zone.teleport.exit", "Destination", "", "", "", "", "", "", "", 0, 0},
        {"Damage / Healing", "Health", "Content/Scripts/Library/renegade.zones.health/HealthZone.lua", "zone.health.enter", "zone.health.exit", "", "", "", "", "", "Damage on entry|Heal on entry|Damage per second|Heal per second", "Health points", "", 10, 0},
        {"Kill / Out of Bounds", "Kill", "Content/Scripts/Library/renegade.zones.kill/KillZone.lua", "zone.kill.enter", "zone.kill.exit", "", "", "", "", "", "Return to checkpoint|Instant death", "", "", 0, 0},
        {"Encounter / Spawn", "Encounter", "Content/Scripts/Library/renegade.zones.encounter/EncounterZone.lua", "zone.encounter.enter", "zone.encounter.exit", "Group / spawner", "Activation event", "", "activate", "", "", "", "", 0, 0},
        {"Dialogue", "Dialogue", "Content/Scripts/Library/renegade.zones.dialogue/DialogueZone.lua", "zone.dialogue.enter", "zone.dialogue.exit", "Speaker", "Dialogue asset", "Entry", "", "", "", "", "", 0, 0},
        {"Audio / Ambience", "Ambience", "Content/Scripts/Library/renegade.zones.ambience/AmbienceZone.lua", "zone.ambience.enter", "zone.ambience.exit", "Sound source", "", "", "", "", "", "", "Fade (s)", 0, 1},
        {"Environment", "Environment", "Content/Scripts/Library/renegade.zones.environment/EnvironmentZone.lua", "zone.environment.enter", "zone.environment.exit", "", "", "", "", "", "Clear|Scattered|Overcast|Storm", "", "Blend (s)", 0, 2}
    }};
    inline const Preset& Info(Behaviour b) { return Presets.at(size_t(b)); }

    // Bounds are in authored local metres. The placement origin is on the floor.
    struct Definition
    {
        std::array<float, 3> low{-1, 0, -1};
        std::array<float, 3> high{1, 2, 1};
        Behaviour behaviour = Behaviour::Trigger;
        std::string primary, secondary;
        int mode = 0, activation = 0, repeat = 0;
        float amount = 0, seconds = 0, cooldown = 1;
        std::string enter = "enter", exit = "exit", target;
    };

    inline constexpr const char* Version = "renegade.zone.version";
    inline constexpr const char* Low[] = {"zone.min.x", "zone.min.y", "zone.min.z"};
    inline constexpr const char* High[] = {"zone.max.x", "zone.max.y", "zone.max.z"};

    inline const char* ScriptPath(Behaviour behaviour)
    {
        return Info(behaviour).source;
    }

    inline const char* EdgeEvent(Behaviour behaviour, bool enter)
    {
        return enter ? Info(behaviour).onEnter : Info(behaviour).onExit;
    }

    inline void Choose(Definition& zone, Behaviour b)
    {
        zone.behaviour = b;
        const auto& preset = Info(b);
        zone.primary = preset.primaryDefault;
        zone.secondary = preset.secondaryDefault;
        zone.amount = preset.amount;
        zone.seconds = preset.seconds;
        zone.mode = 0;
    }

    inline bool IsZone(const Scene& scene, Entity id)
    {
        const auto* m = scene.metadatas.GetComponent(id);
        return m && m->int_values.has(Version) && m->int_values.get(Version) == 1;
    }

    inline bool Valid(const Definition& zone)
    {
        for (size_t i = 0; i < 3; ++i)
            if (!std::isfinite(zone.low[i]) || !std::isfinite(zone.high[i]) ||
                zone.high[i] <= zone.low[i]) return false;
        return true;
    }

    inline bool Read(const Scene& scene, Entity id, Definition& zone)
    {
        if (!IsZone(scene, id)) return false;
        const auto& m = *scene.metadatas.GetComponent(id);
        for (size_t i = 0; i < 3; ++i)
        {
            zone.low[i] = m.float_values.get(Low[i]);
            zone.high[i] = m.float_values.get(High[i]);
        }
        const int code = m.int_values.get("zone.behaviour");
        if (code < 0 || code >= int(Behaviour::Count)) return false;
        Choose(zone, Behaviour(code));
        if (m.string_values.has("zone.primary")) zone.primary = m.string_values.get("zone.primary");
        if (m.string_values.has("zone.secondary")) zone.secondary = m.string_values.get("zone.secondary");
        if (m.float_values.has("zone.amount")) zone.amount = m.float_values.get("zone.amount");
        if (m.float_values.has("zone.seconds")) zone.seconds = m.float_values.get("zone.seconds");
        zone.cooldown = m.float_values.has("zone.cooldown") ? m.float_values.get("zone.cooldown") : 1.f;
        zone.activation = m.int_values.get("zone.activation");
        zone.repeat = m.int_values.get("zone.repeat");
        zone.mode = m.int_values.get("zone.mode");
        zone.enter = m.string_values.get("zone.enter");
        zone.exit = m.string_values.get("zone.exit");
        zone.target = m.string_values.get("zone.target");
        return Valid(zone);
    }

    inline void Write(Scene& scene, Entity id, const Definition& zone)
    {
        auto& m = *scene.metadatas.GetComponent(id);
        m.int_values.set(Version, 1);
        m.int_values.set("zone.behaviour", int(zone.behaviour));
        for (size_t i = 0; i < 3; ++i)
        {
            m.float_values.set(Low[i], zone.low[i]);
            m.float_values.set(High[i], zone.high[i]);
        }
        m.string_values.set("zone.primary", zone.primary);
        m.string_values.set("zone.secondary", zone.secondary);
        m.float_values.set("zone.amount", zone.amount);
        m.float_values.set("zone.seconds", zone.seconds);
        m.float_values.set("zone.cooldown", zone.cooldown);
        m.int_values.set("zone.mode", zone.mode);
        m.int_values.set("zone.activation", zone.activation);
        m.int_values.set("zone.repeat", zone.repeat);
        m.string_values.set("zone.enter", zone.enter);
        m.string_values.set("zone.exit", zone.exit);
        m.string_values.set("zone.target", zone.target);
    }

    inline bool Contains(const Definition& zone, DirectX::FXMMATRIX world, const XMFLOAT3& point)
    {
        using namespace DirectX;
        XMVECTOR determinant;
        const auto inverse = XMMatrixInverse(&determinant, world);
        if (XMVectorGetX(determinant) == 0) return false;
        XMFLOAT3 p;
        XMStoreFloat3(&p, XMVector3TransformCoord(XMLoadFloat3(&point), inverse));
        return p.x >= zone.low[0] && p.x <= zone.high[0] && p.y >= zone.low[1] &&
               p.y <= zone.high[1] && p.z >= zone.low[2] && p.z <= zone.high[2];
    }

    inline bool HitDistance(const Definition& zone, DirectX::FXMMATRIX world,
                            const wi::primitive::Ray& ray, float& distance)
    {
        using namespace DirectX;
        XMVECTOR determinant;
        const auto inverse = XMMatrixInverse(&determinant,world);
        if (XMVectorGetX(determinant) == 0) return false;
        XMFLOAT3 origin, direction;
        XMStoreFloat3(&origin,XMVector3TransformCoord(XMLoadFloat3(&ray.origin),inverse));
        // Keep the local direction unnormalised so t remains a world-space distance.
        XMStoreFloat3(&direction,XMVector3TransformNormal(XMLoadFloat3(&ray.direction),inverse));
        const std::array<float,3> o{origin.x,origin.y,origin.z}, d{direction.x,direction.y,direction.z};
        float nearT = -FLT_MAX, farT = FLT_MAX;
        for (size_t i=0; i<3; ++i)
        {
            if (std::abs(d[i]) < 1e-8f)
            {
                if (o[i] < zone.low[i] || o[i] > zone.high[i]) return false;
                continue;
            }
            const float a = (zone.low[i]-o[i])/d[i], b = (zone.high[i]-o[i])/d[i];
            nearT = std::max(nearT,std::min(a,b));
            farT = std::min(farT,std::max(a,b));
            if (nearT > farT) return false;
        }
        distance = nearT >= ray.TMin ? nearT : farT;
        return distance >= ray.TMin && distance <= ray.TMax;
    }

    class Create final : public ICommand
    {
    public:
        Create(Scene& scene, XMFLOAT3 position) : scene_(scene), position_(position) {}
        Entity Id() const { return id_; }
        const StableId& PersistentId() const { return stableId_; }
        bool Execute() override
        {
            if (scene_.transforms.Contains(id_) || scene_.names.Contains(id_)) return false;
            scene_.names.Create(id_).name = "Gameplay Zone";
            auto& transform = scene_.transforms.Create(id_);
            transform.Translate(position_);
            transform.UpdateTransform();
            std::string error;
            if (!AssignPersistentEntityId(scene_, id_, stableId_, error))
            {
                scene_.Entity_Remove(id_);
                return false;
            }
            Write(scene_, id_, {});
            return true;
        }
        void Undo() override { scene_.Entity_Remove(id_); }
    private:
        Scene& scene_;
        XMFLOAT3 position_;
        Entity id_ = wi::ecs::CreateEntity();
        StableId stableId_ = GenerateStableId();
    };

    class Edit final : public ICommand
    {
    public:
        Edit(Scene& scene, Entity entity, Definition next)
            : scene_(scene), id_(entity), next_(std::move(next)) {}
        bool Execute() override
        {
            if (!Valid(next_) || !Read(scene_, id_, before_)) return false;
            Write(scene_, id_, next_);
            return true;
        }
        void Undo() override { if (IsZone(scene_, id_)) Write(scene_, id_, before_); }
    private:
        Scene& scene_;
        Entity id_;
        Definition next_, before_;
    };
}
