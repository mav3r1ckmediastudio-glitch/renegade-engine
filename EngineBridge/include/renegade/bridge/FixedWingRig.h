#pragma once

#include "FlightVehicleService.h"
#include <array>
#include <vector>

namespace renegade::bridge
{
    enum class FixedWingPart : size_t
    {
        PortAileron, StarboardAileron, Elevator, Rudder, Propeller, Steering,
        NoseWheel, PortWheel, StarboardWheel, PilotEye, NoseContact, PortContact, StarboardContact,
        PortLift, StarboardLift, TailLift, FinForce, ThrustPoint, Count
    };

    struct FixedWingPartInfo
    {
        const char* key;
        const char* fallbackName;
        const char* label;
    };

    inline constexpr std::array<FixedWingPartInfo, static_cast<size_t>(FixedWingPart::Count)> FixedWingParts{{
        {"renegade.fixed_wing.part.port_aileron", "Aileron_L", "LEFT AILERON"},
        {"renegade.fixed_wing.part.starboard_aileron", "Aileron_R", "RIGHT AILERON"},
        {"renegade.fixed_wing.part.elevator", "Elevator", "ELEVATOR"},
        {"renegade.fixed_wing.part.rudder", "Rudder", "RUDDER"},
        {"renegade.fixed_wing.part.propeller", "Propeller_Spin", "PROP / ROTOR VISUAL"},
        {"renegade.fixed_wing.part.steering", "Nose_Steer", "NOSE STEERING PIVOT"},
        {"renegade.fixed_wing.part.nose_wheel", "Wheel_Nose", "NOSE WHEEL"},
        {"renegade.fixed_wing.part.port_wheel", "Wheel_Main_L", "LEFT MAIN WHEEL"},
        {"renegade.fixed_wing.part.starboard_wheel", "Wheel_Main_R", "RIGHT MAIN WHEEL"},
        {"renegade.fixed_wing.part.pilot_eye", "REF_Eye_Pilot", "COCKPIT VIEW POINT"},
        {"renegade.fixed_wing.part.nose_contact", "REF_WheelContact_Nose", "NOSE CONTACT MARKER"},
        {"renegade.fixed_wing.part.port_contact", "REF_WheelContact_Main_L", "LEFT CONTACT MARKER"},
        {"renegade.fixed_wing.part.starboard_contact", "REF_WheelContact_Main_R", "RIGHT CONTACT MARKER"},
        {"renegade.fixed_wing.part.port_lift", "REF_WingLift_L", "LEFT WING FORCE POINT"},
        {"renegade.fixed_wing.part.starboard_lift", "REF_WingLift_R", "RIGHT WING FORCE POINT"},
        {"renegade.fixed_wing.part.tail_lift", "REF_TailLift", "TAIL LIFT FORCE POINT"},
        {"renegade.fixed_wing.part.fin_force", "REF_FinForce", "FIN FORCE POINT"},
        {"renegade.fixed_wing.part.thrust", "REF_Thrust", "THRUST FORCE POINT"},
    }};

    inline bool FixedWingPartTransform(const wi::scene::Scene& scene, wi::ecs::Entity child,
        wi::ecs::Entity chassis, XMMATRIX& transform)
    {
        transform = XMMatrixIdentity();
        for (auto remaining = scene.hierarchy.GetCount() + 1; remaining && child != wi::ecs::INVALID_ENTITY; --remaining)
        {
            if (child == chassis) return true;
            const auto* local = scene.transforms.GetComponent(child);
            const auto* parent = scene.hierarchy.GetComponent(child);
            if (!local || !parent || scene.rigidbodies.Contains(child)) return false;
            transform *= local->GetLocalMatrix();
            child = parent->parentID;
        }
        return false;
    }

    struct FixedWingRig
    {
        std::vector<wi::ecs::Entity> candidates;
        std::array<wi::ecs::Entity, FixedWingParts.size()> automatic{};
        std::array<std::vector<wi::ecs::Entity>, FixedWingParts.size()> assigned;

        wi::ecs::Entity Part(size_t index) const
        {
            if (assigned[index].size() == 1) return assigned[index].front();
            return assigned[index].empty() ? automatic[index] : wi::ecs::INVALID_ENTITY;
        }
    };

    inline FixedWingRig ReadFixedWingRig(const wi::scene::Scene& scene, wi::ecs::Entity chassis)
    {
        FixedWingRig rig;
        rig.automatic.fill(wi::ecs::INVALID_ENTITY);
        std::array<unsigned, FixedWingParts.size()> matches{};
        for (size_t i = 0; i < scene.hierarchy.GetCount(); ++i)
        {
            const auto entity = scene.hierarchy.GetEntity(i);
            XMMATRIX transform;
            if (entity == chassis || !FixedWingPartTransform(scene, entity, chassis, transform))
                continue;
            rig.candidates.push_back(entity);
            const auto* metadata = scene.metadatas.GetComponent(entity);
            const auto* name = scene.names.GetComponent(entity);
            for (size_t part = 0; part < FixedWingParts.size(); ++part)
            {
                const auto& info = FixedWingParts[part];
                if (metadata && metadata->bool_values.has(info.key) && metadata->bool_values.get(info.key))
                    rig.assigned[part].push_back(entity);
                if (name && name->name == info.fallbackName)
                {
                    ++matches[part];
                    rig.automatic[part] = matches[part] == 1 ? entity : wi::ecs::INVALID_ENTITY;
                }
            }
        }
        return rig;
    }

    inline void SetFixedWingPartBinding(wi::scene::Scene& scene, wi::ecs::Entity entity, size_t part, bool enabled)
    {
        auto* metadata = scene.metadatas.GetComponent(entity);
        if (enabled)
        {
            if (!metadata) metadata = &scene.metadatas.Create(entity);
            metadata->bool_values.set(FixedWingParts[part].key, true);
        }
        else if (metadata)
            metadata->bool_values.erase(FixedWingParts[part].key);
    }
}
