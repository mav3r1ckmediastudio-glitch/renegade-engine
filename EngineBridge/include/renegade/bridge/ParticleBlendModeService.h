#pragma once

#include <WickedEngine.h>

#include "renegade/bridge/CommandService.h"
#include "renegade/bridge/MaterialService.h"

namespace renegade::bridge
{
    [[nodiscard]] wi::enums::BLENDMODE CaptureParticleBlendMode(
        const wi::scene::Scene& scene,
        wi::ecs::Entity emitter) noexcept;

    [[nodiscard]] bool IsSupportedParticleBlendMode(
        wi::enums::BLENDMODE blendMode) noexcept;

    class SetParticleBlendModeCommand final : public ICommand
    {
    public:
        SetParticleBlendModeCommand(
            wi::scene::Scene& scene,
            wi::ecs::Entity emitter,
            wi::enums::BLENDMODE blendMode);

        bool Execute() override;
        void Undo() override;

    private:
        bool Apply(const MaterialState& state) noexcept;

        wi::scene::Scene* scene_ = nullptr;
        wi::ecs::Entity emitter_ = wi::ecs::INVALID_ENTITY;
        MaterialState before_;
        MaterialState after_;
    };
}
