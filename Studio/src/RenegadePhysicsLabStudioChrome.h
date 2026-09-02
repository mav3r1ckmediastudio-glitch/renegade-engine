#pragma once

#include <functional>

#include "RenegadeAudioWorkspace.h"
#include "RenegadePhysicsLabWorkspace.h"
#include "RenegadeStudioChrome.h"

namespace renegade::studio
{
    // Thin shell integration for Physics Lab and Gate 3 Audio. StudioApplication
    // continues to own one chrome widget; both bounded authoring surfaces remain
    // isolated here rather than leaking their state into StudioApplication.
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
        [[nodiscard]] bool IsAudioWorkspaceActive() const noexcept
        {
            return audioWorkspace_.IsActive();
        }
        [[nodiscard]] RenegadeAudioWorkspace& AudioWorkspace() noexcept
        {
            return audioWorkspace_;
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
        void SetAudioWorkspaceActive(bool active);
        void SynchronizeRigidBodyOwnerSelection();
        [[nodiscard]] bool PhysicsTabHit(const XMFLOAT4& pointer) const noexcept;
        [[nodiscard]] XMFLOAT4 AudioViewportToolBounds() const noexcept;
        [[nodiscard]] bool AudioViewportToolHit(
            const XMFLOAT4& pointer) const noexcept;
        [[nodiscard]] XMFLOAT4 AudioInspectorBounds() const noexcept;
        void RenderPhysicsTab(wi::graphics::CommandList cmd) const;
        void RenderAudioViewportTool(wi::graphics::CommandList cmd) const;

        RenegadePhysicsLabWorkspace physicsLab_;
        RenegadeAudioWorkspace audioWorkspace_;
        std::function<void(Action)> studioAction_;
        bool physicsTabConsumed_ = false;
        bool audioToolConsumed_ = false;
        bool workspaceTransitionRequested_ = false;
    };
}
