#pragma once

#include <memory>

#include <WickedEngine.h>

namespace renegade::studio
{
    // Creator-facing authoring surface over Wicked's native VoxelGrid. The
    // workspace owns presentation only; EngineBridge NavigationService remains
    // the mutation/query boundary and Wicked remains the serialized authority.
    class RenegadeNavigationWorkspace final : public wi::gui::Widget
    {
    public:
        RenegadeNavigationWorkspace();
        ~RenegadeNavigationWorkspace();

        RenegadeNavigationWorkspace(const RenegadeNavigationWorkspace&) = delete;
        RenegadeNavigationWorkspace& operator=(const RenegadeNavigationWorkspace&) = delete;
        RenegadeNavigationWorkspace(RenegadeNavigationWorkspace&&) = delete;
        RenegadeNavigationWorkspace& operator=(RenegadeNavigationWorkspace&&) = delete;

        void Create();
        void SetActive(bool active);
        [[nodiscard]] bool IsActive() const noexcept;
        void SetBounds(const XMFLOAT4& bounds);
        [[nodiscard]] bool ContainsPointer(const XMFLOAT4& pointer) const noexcept;
        [[nodiscard]] bool ConsumedPointerThisFrame() const noexcept;
        [[nodiscard]] bool HasSelectedNavigationGrid() const noexcept;
        void CreateNavigationGridForScene();

        void Update(const wi::Canvas& canvas, float dt) override;
        void Render(
            const wi::Canvas& canvas,
            wi::graphics::CommandList cmd) const override;
        const char* GetWidgetTypeName() const override
        {
            return "RenegadeNavigationWorkspace";
        }

    private:
        struct Impl;
        std::unique_ptr<Impl> impl_;
    };
}
