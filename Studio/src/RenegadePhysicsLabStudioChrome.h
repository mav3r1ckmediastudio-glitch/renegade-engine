#pragma once

#include <functional>

#include "RenegadePhysicsLabWorkspace.h"
#include "RenegadeStudioChrome.h"

namespace renegade::studio
{
    // Thin shell integration for Physics Lab. StudioApplication continues to
    // own one chrome widget; physics presentation and state remain isolated in
    // RenegadePhysicsLabWorkspace rather than leaking into StudioApplication.
    class RenegadePhysicsLabStudioChrome final : public CreatorAssetStudioChrome
    {
    public:
        void Create();
        void SetLayout(float width, float height);
        void OnAction(std::function<void(Action)> callback);
        void RequestCurrentWorkspaceReconcile();

        [[nodiscard]] bool IsPhysicsLabActive() const noexcept
        {
            return physicsLab_.IsActive();
        }
        [[nodiscard]] bool ConsumedPointerThisFrame() const noexcept;

        void Update(const wi::Canvas& canvas, float dt) override;
        void Render(
            const wi::Canvas& canvas,
            wi::graphics::CommandList cmd) const override;
        const char* GetWidgetTypeName() const override
        {
            return "RenegadePhysicsLabStudioChrome";
        }

    private:
        void SetPhysicsLabActive(bool active);
        void RefreshSelectedCollisionScale();
        [[nodiscard]] bool PhysicsTabHit(const XMFLOAT4& pointer) const noexcept;
        void RenderPhysicsTab(wi::graphics::CommandList cmd) const;

        RenegadePhysicsLabWorkspace physicsLab_;
        std::function<void(Action)> studioAction_;
        wi::ecs::Entity observedPhysicsScaleEntity_ = wi::ecs::INVALID_ENTITY;
        XMFLOAT3 observedPhysicsScale_ = XMFLOAT3(1.0f, 1.0f, 1.0f);
        bool observedPhysicsScaleValid_ = false;
        bool physicsTabConsumed_ = false;
    };
}
