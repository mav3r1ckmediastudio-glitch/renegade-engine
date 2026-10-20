#pragma once

// A quiet-period save trigger; no filesystem access and no document mutation.
// Failed writes are never acknowledged as saved and are not retried every
// frame. A new edit or an explicit Save can retry the transaction.
#include <algorithm>
#include <cmath>
#include <cstddef>

namespace renegade::studio
{
    class ScreenEditorAutosavePolicy
    {
    public:
        static constexpr double QuietSeconds = 1.5;

        void Reset() noexcept
        {
            observed_ = false;
            lastDirty_ = false;
            lastUndo_ = 0;
            lastRedo_ = 0;
            elapsed_ = 0.0;
            attempted_ = false;
        }

        void Edited() noexcept
        {
            elapsed_ = 0.0;
            attempted_ = false;
        }

        void Observe(const bool dirty, const std::size_t undo,
            const std::size_t redo, const double deltaSeconds,
            const bool editing) noexcept
        {
            if (!observed_ || lastDirty_ != dirty ||
                lastUndo_ != undo || lastRedo_ != redo)
            {
                observed_ = true;
                lastDirty_ = dirty;
                lastUndo_ = undo;
                lastRedo_ = redo;
                Edited();
            }
            if (!dirty)
            {
                elapsed_ = 0.0;
                attempted_ = false;
                return;
            }
            if (editing)
            {
                elapsed_ = 0.0;
                return;
            }
            if (std::isfinite(deltaSeconds) && deltaSeconds > 0.0)
                elapsed_ += std::min(deltaSeconds, 0.25);
        }

        [[nodiscard]] bool Due() const noexcept
        {
            return observed_ && lastDirty_ && !attempted_ &&
                elapsed_ >= QuietSeconds;
        }

        void Attempted() noexcept
        {
            attempted_ = true;
        }

    private:
        bool observed_ = false;
        bool lastDirty_ = false;
        std::size_t lastUndo_ = 0;
        std::size_t lastRedo_ = 0;
        double elapsed_ = 0.0;
        bool attempted_ = false;
    };
}
