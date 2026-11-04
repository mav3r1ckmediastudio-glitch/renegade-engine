#pragma once
#include <WickedEngine.h>
#include <memory>
#include <utility>

namespace renegade::studio::detail
{
    // A cancelled gesture owns its mouse-up even after its preview is gone.
    // Both release intake and boundary processing use this same main-thread gate.
    class CreatorAssetDragCancellation
    {
        bool cancelledUntilRelease_ = false;
    public:
        void Cancel(bool leftDown) noexcept { cancelledUntilRelease_ = leftDown; }
        void Reset() noexcept { cancelledUntilRelease_ = false; }
        bool BlocksDrop(bool cancelPressed = false) const noexcept { return cancelPressed || cancelledUntilRelease_; }
        bool SuppressFrame(bool leftPressed, bool leftReleased, bool leftDown) noexcept
        {
            if (leftPressed) { Reset(); return false; }
            if (!cancelledUntilRelease_) return false;
            if (leftReleased || !leftDown) Reset();
            return true;
        }
    };

    // Used only on the Studio/main thread. The native update that follows this
    // boundary publishes mesh_index and GPU descriptors before shadow rendering.
    struct CreatorAssetFrameMutation
    {
        std::shared_ptr<bool> pending;
        void Cancel() const noexcept { if (pending) *pending = false; }
        bool IsPending() const noexcept { return pending && *pending; }
    };

    template<typename Callback>
    CreatorAssetFrameMutation QueueCreatorAssetFrameMutation(Callback callback)
    {
        CreatorAssetFrameMutation ticket{std::make_shared<bool>(true)};
        wi::eventhandler::Subscribe_Once(wi::eventhandler::EVENT_THREAD_SAFE_POINT,
            [pending = ticket.pending, callback = std::move(callback)](
                std::uint64_t value) mutable
        {
            if (!*pending) return;
            *pending = false;
            callback(value);
        });
        return ticket;
    }
}
