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
        void SynchronizeRigidBodyAuthoringTarget();
        [[nodiscard]] bool PhysicsTabHit(const XMFLOAT4& pointer) const noexcept;
        void RenderPhysicsTab(wi::graphics::CommandList cmd) const;

        RenegadePhysicsLabWorkspace physicsLab_;
        std::function<void(Action)> studioAction_;
        bool physicsTabConsumed_ = false;

        // The primitive dimensions stored on a reusable-asset root deliberately
        // exclude wrapper scale. Track that authored scale separately and ask
        // Wicked/Jolt to recreate its implementation-owned shape when it moves.
        wi::scene::Scene* trackedPhysicsScaleScene_ = nullptr;
        wi::ecs::Entity trackedPhysicsScaleEntity_ = wi::ecs::INVALID_ENTITY;
        XMFLOAT3 trackedPhysicsScale_ = XMFLOAT3(1.0f, 1.0f, 1.0f);
        bool trackedPhysicsScaleValid_ = false;
    };
}
