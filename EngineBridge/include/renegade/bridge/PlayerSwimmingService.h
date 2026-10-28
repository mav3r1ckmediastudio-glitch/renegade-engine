#pragma once
#include "renegade/bridge/PlayerService.h"
namespace renegade::bridge
{
    // Swim travel is 25% slower than the same authored walk/sprint speed.
    inline constexpr float PlayerSwimSpeedMultiplier = 0.75f;
    // Uses the evaluated camera angles. Land movement keeps its horizontal rule.
    XMFLOAT3 EvaluatePlayerSwimmingVelocity(const PlayerInputFrame& input,
        float yaw,float pitch,float feetY,float surfaceY,
        const PlayerControllerSettings& settings = {}) noexcept;
    // Returns true only when the native character was moved in a linked river.
    // Restores the character's original buoyancy when leaving swimming.
    bool TryMovePlayerInRiver(wi::scene::Scene& scene,RuntimePlayerState& state,
        const PlayerInputFrame& input,const PlayerControllerSettings& settings) noexcept;
}
