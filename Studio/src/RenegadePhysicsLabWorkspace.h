#pragma once

#include <cstdint>
#include <memory>

#include <WickedEngine.h>

namespace renegade::studio
{
    // JP01 creator-facing Physics Lab. This is presentation/orchestration only:
    // Wicked Scene components remain the serialized authority and the existing
    // Wicked/Jolt world remains the only live physics world.
    class RenegadePhysicsLabWorkspace final
    {
    public:
        enum class Page : std::uint8_t
        {
            World,
            RigidBody,
            Constraint,
            Character,
            Vehicle,
            Ragdoll,
            SoftBody,
            WickedCollider,
            Count,
        };

        RenegadePhysicsLabWorkspace();
        ~RenegadePhysicsLabWorkspace();

        RenegadePhysicsLabWorkspace(const RenegadePhysicsLabWorkspace&) = delete;
        RenegadePhysicsLabWorkspace& operator=(const RenegadePhysicsLabWorkspace&) = delete;
        RenegadePhysicsLabWorkspace(RenegadePhysicsLabWorkspace&&) = delete;
        RenegadePhysicsLabWorkspace& operator=(RenegadePhysicsLabWorkspace&&) = delete;

        void Create();
        void SetActive(bool active);
        [[nodiscard]] bool IsActive() const noexcept;
        void SetBounds(const XMFLOAT4& bounds);
        [[nodiscard]] bool ContainsPointer(const XMFLOAT4& pointer) const noexcept;
        [[nodiscard]] bool ConsumedPointerThisFrame() const noexcept;
        void Update(const wi::Canvas& canvas, float dt);
        void Render(const wi::Canvas& canvas, wi::graphics::CommandList cmd) const;
        [[nodiscard]] Page ActivePage() const noexcept;

    private:
        struct Impl;
        std::unique_ptr<Impl> impl_;
    };
}
