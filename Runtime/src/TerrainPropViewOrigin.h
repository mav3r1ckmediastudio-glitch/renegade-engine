#pragma once
#include "renegade/bridge/PhysicsService.h"
#include <optional>

namespace renegade::runtime
{
    inline std::optional<XMFLOAT3> TerrainPropViewOrigin(wi::scene::Scene& scene,
        wi::ecs::Entity vehicle, wi::ecs::Entity player, const XMFLOAT3* camera)
    {
        // Boarding leaves the walking body at the entry point until the player exits.
        const bool riding = vehicle != wi::ecs::INVALID_ENTITY;
        const auto body = riding ? vehicle : player;
        XMFLOAT3 position{};
        if (body != wi::ecs::INVALID_ENTITY && bridge::GetPhysicsPosition(scene, body, position))
            return position;
        if (riding)
        {
            if (const auto* chassis = scene.transforms.GetComponent(vehicle))
                return chassis->GetPosition();
        }
        if (camera) return *camera;
        return std::nullopt;
    }
}
