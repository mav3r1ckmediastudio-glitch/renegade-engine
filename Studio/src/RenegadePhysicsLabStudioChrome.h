#pragma once

#include <functional>

#include "RenegadeAudioWorkspace.h"
#include "RenegadeNavigationWorkspace.h"
#include "RenegadeParticleEmitterWorkspace.h"
#include "RenegadePhysicsLabWorkspace.h"
#include "RenegadeStudioChrome.h"

namespace renegade::studio
{
    // Thin shell integration for Physics Lab, Audio, particles and native
    // Wicked navigation authoring. StudioApplication continues to own one
    // chrome widget; bounded specialist surfaces remain isolated here rather
    // than leaking their feature state into StudioApplication.
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
        [[nodiscard]] bool IsParticleWorkspaceActive() const noexcept
        {
            return particleWorkspace_.IsActive();
        }
        [[nodiscard]] bool IsNavigationWorkspaceActive() const noexcept
        {
            return navigationWorkspace_.IsActive();
        }
        // StudioApplication's existing special-Inspector ownership check is
        // named for Audio. Treat particles and navigation as the same right-
        // panel ownership class so the ordinary Inspector is hidden while any
        // specialist Inspector is active.
        [[nodiscard]] bool IsAudioWorkspaceActive() const noexcept
        {
            return audioWorkspace_.IsActive() ||
                particleWorkspace_.IsActive() ||
                navigationWorkspace_.IsActive();
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
        void SetParticleWorkspaceActive(bool active);
        void SetNavigationWorkspaceActive(bool active);
        void SynchronizeRigidBodyOwnerSelection();
        [[nodiscard]] bool PhysicsTabHit(const XMFLOAT4& pointer) const noexcept;
        [[nodiscard]] XMFLOAT4 AudioViewportToolBounds() const noexcept;
        [[nodiscard]] bool AudioViewportToolHit(
            const XMFLOAT4& pointer) const noexcept;
        [[nodiscard]] XMFLOAT4 AudioInspectorBounds() const noexcept;
        [[nodiscard]] bool AddMenuLabelHit(const XMFLOAT4& pointer) const noexcept;
        [[nodiscard]] XMFLOAT4 ParticleAddMenuItemBounds() const noexcept;
        [[nodiscard]] bool ParticleAddMenuItemHit(
            const XMFLOAT4& pointer) const noexcept;
        [[nodiscard]] XMFLOAT4 NavigationAddMenuItemBounds() const noexcept;
        [[nodiscard]] bool NavigationAddMenuItemHit(
            const XMFLOAT4& pointer) const noexcept;
        void RenderParticleAddMenuItem(wi::graphics::CommandList cmd) const;
        void RenderNavigationAddMenuItem(wi::graphics::CommandList cmd) const;
        void RenderPhysicsTab(wi::graphics::CommandList cmd) const;
        void RenderAudioViewportTool(wi::graphics::CommandList cmd) const;

        RenegadePhysicsLabWorkspace physicsLab_;
        RenegadeAudioWorkspace audioWorkspace_;
        RenegadeParticleEmitterWorkspace particleWorkspace_;
        RenegadeNavigationWorkspace navigationWorkspace_;
        std::function<void(Action)> studioAction_;
        bool physicsTabConsumed_ = false;
        bool audioToolConsumed_ = false;
        bool particleMenuConsumed_ = false;
        bool navigationMenuConsumed_ = false;
        bool particleAddMenuOpen_ = false;
        bool workspaceTransitionRequested_ = false;
    };
}
