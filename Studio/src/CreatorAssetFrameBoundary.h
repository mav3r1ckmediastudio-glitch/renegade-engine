#pragma once
#include <WickedEngine.h>
#include <memory>
#include <utility>

namespace renegade::studio::detail
{
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
