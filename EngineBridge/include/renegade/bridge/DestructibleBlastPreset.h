#pragma once

#include "PhysicsGameplayService.h"

namespace renegade::bridge
{
    inline constexpr float StandardDestructibleBlastSpeed = 5.5f;

    wi::ecs::Entity ResolveAuthoredDestructibleRoot(const wi::scene::Scene& scene, wi::ecs::Entity entity);
    bool CanUseDestructibleBlastPreset(const wi::scene::Scene& scene, wi::ecs::Entity root);

    class SetDestructibleBlastPresetCommand final : public ICommand
    {
    public:
        SetDestructibleBlastPresetCommand(wi::scene::Scene& scene, wi::ecs::Entity entity, bool enabled)
            : scene_(scene), entity_(entity), before_(GetDestructibleBlastSpeed(scene, entity)), enabled_(enabled)
        {}
        bool Execute() override;
        void Undo() override { SetDestructibleBlastSpeed(scene_, entity_, before_); }

    private:
        wi::scene::Scene& scene_;
        wi::ecs::Entity entity_;
        float before_;
        bool enabled_;
    };
}
