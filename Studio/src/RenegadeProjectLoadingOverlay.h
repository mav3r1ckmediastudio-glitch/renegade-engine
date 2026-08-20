#pragma once

#include <atomic>
#include <cstddef>
#include <functional>
#include <string>

#include <WickedEngine.h>

namespace renegade::studio
{
    class RenegadeProjectLoadingOverlay final : public wi::gui::Widget
    {
    public:
        enum class Phase : int
        {
            Idle = 0,
            ValidatingProject,
            PreparingScene,
            RestoringAssets,
            Finalising,
            Ready,
            Failed,
        };

        void Create();
        void SetLayout(float width, float height);
        void Begin(std::string projectLabel);
        void SetPhase(Phase phase, std::size_t completed = 0, std::size_t total = 0) noexcept;
        void Fail(std::string error);
        void OnReturnToHub(std::function<void()> callback);

        [[nodiscard]] Phase CurrentPhase() const noexcept;
        [[nodiscard]] bool IsBlocking() const noexcept;

        void Update(const wi::Canvas& canvas, float dt) override;
        void Render(const wi::Canvas& canvas, wi::graphics::CommandList cmd) const override;
        const char* GetWidgetTypeName() const override
        {
            return "RenegadeProjectLoadingOverlay";
        }

    private:
        [[nodiscard]] XMFLOAT4 ReturnButtonBounds() const noexcept;
        [[nodiscard]] float PhaseProgress(Phase phase, std::size_t completed, std::size_t total) const noexcept;
        [[nodiscard]] std::string PhaseMessage(Phase phase) const;
        [[nodiscard]] std::string PhaseDetail(Phase phase, std::size_t completed, std::size_t total) const;

        float width_ = 1920.0f;
        float height_ = 1080.0f;
        float scale_ = 1.0f;
        float offsetX_ = 0.0f;
        float offsetY_ = 0.0f;
        float elapsed_ = 0.0f;
        float readyElapsed_ = 0.0f;
        std::string projectLabel_;
        std::string error_;
        std::atomic<int> phase_{static_cast<int>(Phase::Idle)};
        std::atomic<std::size_t> completed_{0};
        std::atomic<std::size_t> total_{0};
        std::function<void()> returnToHub_;
    };
}
