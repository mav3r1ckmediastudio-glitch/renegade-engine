#pragma once

#include <memory>

#include <WickedEngine.h>

namespace renegade::studio
{
    // Gate 3 creator-facing audio authoring. This owns presentation and
    // orchestration only; native Wicked SoundComponents and WISCENE metadata
    // remain the serialized authority through EngineBridge AudioService.
    class RenegadeAudioWorkspace final : public wi::gui::Widget
    {
    public:
        RenegadeAudioWorkspace();
        ~RenegadeAudioWorkspace();

        RenegadeAudioWorkspace(const RenegadeAudioWorkspace&) = delete;
        RenegadeAudioWorkspace& operator=(const RenegadeAudioWorkspace&) = delete;
        RenegadeAudioWorkspace(RenegadeAudioWorkspace&&) = delete;
        RenegadeAudioWorkspace& operator=(RenegadeAudioWorkspace&&) = delete;

        void Create();
        void SetActive(bool active);
        [[nodiscard]] bool IsActive() const noexcept;
        void SetBounds(const XMFLOAT4& bounds);
        [[nodiscard]] bool ContainsPointer(const XMFLOAT4& pointer) const noexcept;
        [[nodiscard]] bool ConsumedPointerThisFrame() const noexcept;
        [[nodiscard]] bool HasSelectedSoundSource() const noexcept;
        void CreateSoundSource();
        void Update(const wi::Canvas& canvas, float dt) override;
        void Render(
            const wi::Canvas& canvas,
            wi::graphics::CommandList cmd) const override;
        const char* GetWidgetTypeName() const override
        {
            return "RenegadeAudioWorkspace";
        }

    private:
        struct Impl;
        std::unique_ptr<Impl> impl_;
    };
}
