#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace renegade::studio
{
    // Presentation-only chronological Undo dispatch across the existing Screen
    // and StoryFlow authoring sessions. Both documents keep their own
    // validated history; this class owns no document, route, or disk writes.
    class ScreenEditorUndoTimeline
    {
    public:
        enum class Kind : std::uint8_t { None, Screen, FlowRoute };

        void Reset(const std::size_t screenUndo, const std::size_t screenRedo, const std::uint64_t mutation = 0)
        {
            mutation_ = mutation;
            const auto total = std::min<std::size_t>(512, screenUndo + screenRedo);
            entries_.assign(total, Kind::Screen);
            cursor_ = std::min(screenUndo, total);
            screenUndo_ = screenUndo;
            screenRedo_ = screenRedo;
            mutation_ = mutation;
        }

        // Observe screen edits coalesced by ScreenAuthoringSession. A reset on
        // unexpected external Undo/Redo avoids dispatching to the wrong
        // authority. An ordinary new mutation adds exactly one Screen step.
        void ObserveScreen(const std::size_t undo, const std::size_t redo, const std::uint64_t mutation = 0)
        {
            if (mutation && mutation != mutation_)
            {
                const auto added = mutation > mutation_ ? mutation-mutation_ : 1;
                mutation_ = mutation;
                entries_.resize(cursor_);
                entries_.insert(entries_.end(), std::min<std::uint64_t>(added,undo), Kind::Screen);
                cursor_ = entries_.size();
                // The oldest Screen step is evicted independently of route steps.
                auto screens = std::count(entries_.begin(), entries_.end(), Kind::Screen);
                while (screens > undo)
                {
                    const auto first = std::find(entries_.begin(), entries_.end(), Kind::Screen);
                    entries_.erase(first); --cursor_; --screens;
                }
                screenUndo_ = undo; screenRedo_ = redo; Trim(); return;
            }
            if (undo == screenUndo_ && redo == screenRedo_) return;
            if (undo < screenUndo_ ||
                (undo == screenUndo_ && redo != screenRedo_))
            {
                Reset(undo, redo, mutation);
                return;
            }
            if (undo > screenUndo_)
            {
                entries_.resize(cursor_);
                entries_.insert(entries_.end(), undo - screenUndo_, Kind::Screen);
                cursor_ = entries_.size();
                screenUndo_ = undo;
                screenRedo_ = redo;
                Trim();
            }
        }

        void SyncMutation(std::uint64_t mutation) noexcept { mutation_=mutation; }
        void CancelScreen(const std::size_t undo,const std::size_t redo,const std::uint64_t mutation)
        {
            if(cursor_ && entries_[cursor_-1]==Kind::Screen){entries_.erase(entries_.begin()+static_cast<std::ptrdiff_t>(cursor_-1));--cursor_;}
            screenUndo_=undo;screenRedo_=redo;mutation_=mutation;
        }
        void RecordFlowRoute()
        {
            entries_.resize(cursor_);
            entries_.push_back(Kind::FlowRoute);
            cursor_ = entries_.size();
            Trim();
        }

        [[nodiscard]] Kind NextUndo() const noexcept
        {
            return cursor_ ? entries_[cursor_ - 1] : Kind::None;
        }

        [[nodiscard]] Kind NextRedo() const noexcept
        {
            return cursor_ < entries_.size() ? entries_[cursor_] : Kind::None;
        }

        void DidUndo(const std::size_t screenUndo, const std::size_t screenRedo, const std::uint64_t mutation = 0)
        {
            if (cursor_) --cursor_;
            screenUndo_ = screenUndo;
            screenRedo_ = screenRedo;
            mutation_ = mutation;
        }

        void DidRedo(const std::size_t screenUndo, const std::size_t screenRedo, const std::uint64_t mutation = 0)
        {
            if (cursor_ < entries_.size()) ++cursor_;
            screenUndo_ = screenUndo;
            screenRedo_ = screenRedo;
            mutation_ = mutation;
        }

    private:
        void Trim()
        {
            constexpr std::size_t Limit = 512;
            if (entries_.size() <= Limit) return;
            const auto excess = entries_.size() - Limit;
            entries_.erase(entries_.begin(),
                entries_.begin() + static_cast<std::ptrdiff_t>(excess));
            cursor_ = cursor_ > excess ? cursor_ - excess : 0;
        }

        std::vector<Kind> entries_;
        std::size_t cursor_ = 0;
        std::size_t screenUndo_ = 0;
        std::size_t screenRedo_ = 0;
        std::uint64_t mutation_ = 0;
    };
}
