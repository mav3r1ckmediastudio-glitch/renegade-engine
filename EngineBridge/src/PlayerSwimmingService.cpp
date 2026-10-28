#include "renegade/bridge/PlayerSwimmingService.h"
#include "renegade/bridge/RiverUnderwaterService.h"
#include <algorithm>
#include <cmath>
namespace renegade::bridge
{
    XMFLOAT3 EvaluatePlayerSwimmingVelocity(const PlayerInputFrame& input,
        float yaw,float pitch,float feetY,float surfaceY,const PlayerControllerSettings& settings) noexcept
    {
        const auto safe=SanitizePlayerControllerSettings(settings);
        yaw=std::isfinite(yaw)?yaw:0;pitch=std::isfinite(pitch)?pitch:0;
        float forward=std::isfinite(input.moveForward)?std::clamp(input.moveForward,-1.f,1.f):0;
        float right=std::isfinite(input.moveRight)?std::clamp(input.moveRight,-1.f,1.f):0;
        const float length=std::sqrt(forward*forward+right*right);
        if(length>1){forward/=length;right/=length;}
        const float speed=(input.sprintDown?safe.sprintSpeed:safe.walkSpeed)*PlayerSwimSpeedMultiplier;
        XMFLOAT3 velocity{
            speed*(forward*std::sin(yaw)*std::cos(pitch)+right*std::cos(yaw)),
            -speed*forward*std::sin(pitch),
            speed*(forward*std::cos(yaw)*std::cos(pitch)-right*std::sin(yaw))};
        // Buoyancy is a gentle idle return to a breathing position, never an
        // upward correction that fights an intentional forward dive.
        if(std::abs(forward)<.01f && std::isfinite(feetY)&&std::isfinite(surfaceY))
            velocity.y=std::clamp((surfaceY-safe.eyeHeight*.8f-feetY)*1.5f,-.25f,.35f);
        if(input.jumpPressed)velocity.y=std::max(velocity.y,1.5f);
        return velocity;
    }
    bool TryMovePlayerInRiver(wi::scene::Scene& scene,RuntimePlayerState& state,
        const PlayerInputFrame& input,const PlayerControllerSettings& settings) noexcept
    {
        auto* body=scene.rigidbodies.GetComponent(state.entity);
        if(!body||!body->IsCharacterPhysics()||!body->physicsobject)return false;
        const auto restore=[&](){if(state.swimming){body->buoyancy=state.buoyancyBeforeSwimming;state.swimming=false;}};
        try
        {
            const auto safe=SanitizePlayerControllerSettings(settings);
            const auto feet=wi::physics::GetPosition(*body);
            // Enter at waist submersion, before native buoyancy can hold the
            // character above neck depth. A lower exit sample supplies hysteresis.
            XMFLOAT3 sample{feet.x,feet.y+safe.eyeHeight*(state.swimming?.3f:.4f),feet.z};
            float surface=-INFINITY;
            for(const auto& prism:CaptureRiverWaterPrisms(scene))
            {
                float entry,exit;
                if(IntersectRiverWaterPrism(prism,sample,{0,1,0},10,entry,exit)&&entry<.0001f)
                    surface=std::max(surface,sample.y+exit);
            }
            if(!std::isfinite(surface)){restore();return false;}
            if(!state.swimming){state.buoyancyBeforeSwimming=body->buoyancy;state.swimming=true;}
            // Only this transient Runtime player's lift is disabled. Native Jolt
            // gravity, capsule and solid collisions remain active; no teleport.
            body->buoyancy=0;
            wi::physics::SetLinearVelocity(*body,EvaluatePlayerSwimmingVelocity(input,
                state.yaw,state.pitch,feet.y,surface,safe));
            return true;
        }
        catch(...){restore();return false;}
    }
}
