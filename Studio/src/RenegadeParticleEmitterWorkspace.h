#pragma once

#include <memory>

#include <WickedEngine.h>

namespace renegade::studio
{
    // Creator-facing wrapper around Wicked's native EmittedParticleSystem.
    // The widget owns presentation/orchestration only; WISCENE emitter,
    // material, transform and hierarchy components remain authoritative.
    class RenegadeParticleEmitterWorkspace final : public wi::gui::Widget
    {
    public:
        RenegadeParticleEmitterWorkspace();
        ~RenegadeParticleEmitterWorkspace();

        RenegadeParticleEmitterWorkspace(const RenegadeParticleEmitterWorkspace&) = delete;
        RenegadeParticleEmitterWorkspace& operator=(const RenegadeParticleEmitterWorkspace&) = delete;
        RenegadeParticleEmitterWorkspace(RenegadeParticleEmitterWorkspace&&) = delete;
        RenegadeParticleEmitterWorkspace& operator=(RenegadeParticleEmitterWorkspace&&) = delete;

        void Create();
        void SetActive(bool active);
        [[nodiscard]] bool IsActive() const noexcept;
        void SetBounds(const XMFLOAT4& bounds);
        [[nodiscard]] bool ContainsPointer(const XMFLOAT4& pointer) const noexcept;
        [[nodiscard]] bool ConsumedPointerThisFrame() const noexcept;
        [[nodiscard]] bool HasSelectedEmitter() const noexcept;
        void CreateEmitterInFrontOfCamera();

        void Update(const wi::Canvas& canvas, float dt) override;
        void Render(
            const wi::Canvas& canvas,
            wi::graphics::CommandList cmd) const override;
        const char* GetWidgetTypeName() const override
        {
            return "RenegadeParticleEmitterWorkspace";
        }

    private:
        struct Impl;
        std::unique_ptr<Impl> impl_;
    };
}
