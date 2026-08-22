#include "renegade/bridge/ScreenAuthoringSession.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace
{
    bool SameRect(
        const renegade::bridge::ScreenRect& left,
        const renegade::bridge::ScreenRect& right) noexcept
    {
        return left.x == right.x && left.y == right.y &&
            left.width == right.width && left.height == right.height;
    }

    bool IsFiniteRect(const renegade::bridge::ScreenRect& rect) noexcept
    {
        return std::isfinite(rect.x) && std::isfinite(rect.y) &&
            std::isfinite(rect.width) && std::isfinite(rect.height) &&
            rect.width > 0.0f && rect.height > 0.0f;
    }
}

namespace renegade::bridge
{
    bool ScreenAuthoringSession::Open(
        const std::string& filePath,
        const StableId& expectedProjectId,
        std::string& error)
    {
        ScreenDocument document;
        if (!ReadScreenDocument(filePath, expectedProjectId, document, error))
            return false;
        return Adopt(
            std::move(document), expectedProjectId, filePath, error);
    }

    bool ScreenAuthoringSession::Reload(std::string& error)
    {
        if (!loaded_ || filePath_.empty())
        {
            error = "Screen authoring session is not open.";
            return false;
        }
        ScreenDocument document;
        if (!ReadScreenDocument(filePath_, projectId_, document, error))
            return false;
        return Adopt(std::move(document), projectId_, filePath_, error);
    }

    bool ScreenAuthoringSession::Save(std::string& error)
    {
        if (!loaded_ || filePath_.empty())
        {
            error = "Screen authoring session is not open.";
            return false;
        }
        if (!WriteScreenDocument(filePath_, Document(), error))
            return false;
        savedHistoryIndex_ = historyIndex_;
        error.clear();
        return true;
    }

    void ScreenAuthoringSession::Clear() noexcept
    {
        history_.clear();
        historyIndex_ = 0;
        savedHistoryIndex_ = InvalidHistoryIndex;
        projectId_.clear();
        filePath_.clear();
        loaded_ = false;
    }

    bool ScreenAuthoringSession::UpdateWidget(
        const StableId& widgetId,
        ScreenWidgetAuthoringEdit edit,
        std::string& error)
    {
        if (!loaded_)
        {
            error = "Screen authoring session is not open.";
            return false;
        }
        if (!IsFiniteRect(edit.resolvedRect))
        {
            error = "Screen widget position and size must be finite and positive.";
            return false;
        }

        ScreenDocument candidate = Document();
        const auto found = std::find_if(
            candidate.widgets.begin(), candidate.widgets.end(),
            [&widgetId](const ScreenWidget& widget)
            {
                return widget.id == widgetId;
            });
        if (found == candidate.widgets.end())
        {
            error = "Screen widget does not exist: " + widgetId;
            return false;
        }

        ScreenRect currentResolved;
        if (!ResolveScreenWidgetRect(
                candidate, widgetId, currentResolved, error))
            return false;

        const bool same = found->name == edit.name &&
            found->text == edit.text && found->visible == edit.visible &&
            found->enabled == edit.enabled &&
            SameRect(currentResolved, edit.resolvedRect);
        if (same)
        {
            error.clear();
            return true;
        }

        found->name = std::move(edit.name);
        found->text = std::move(edit.text);
        found->visible = edit.visible;
        found->enabled = edit.enabled;

        if (found->layoutMode == ScreenLayoutMode::Absolute)
        {
            found->rect = edit.resolvedRect;
        }
        else
        {
            ScreenRect parentRect{0.0f, 0.0f,
                candidate.designWidth, candidate.designHeight};
            if (!found->parentId.empty() &&
                !ResolveScreenWidgetRect(
                    candidate, found->parentId, parentRect, error))
            {
                return false;
            }
            const float relativeX = edit.resolvedRect.x - parentRect.x;
            const float relativeY = edit.resolvedRect.y - parentRect.y;
            found->anchors.offsetMinimumX = relativeX -
                found->anchors.minimumX * parentRect.width;
            found->anchors.offsetMinimumY = relativeY -
                found->anchors.minimumY * parentRect.height;
            found->anchors.offsetMaximumX =
                relativeX + edit.resolvedRect.width -
                found->anchors.maximumX * parentRect.width;
            found->anchors.offsetMaximumY =
                relativeY + edit.resolvedRect.height -
                found->anchors.maximumY * parentRect.height;
        }

        return CommitMutation(std::move(candidate), error);
    }

    bool ScreenAuthoringSession::Undo(std::string& error)
    {
        if (!CanUndo())
        {
            error = "Screen has no authoring command to undo.";
            return false;
        }
        --historyIndex_;
        error.clear();
        return true;
    }

    bool ScreenAuthoringSession::Redo(std::string& error)
    {
        if (!CanRedo())
        {
            error = "Screen has no authoring command to redo.";
            return false;
        }
        ++historyIndex_;
        error.clear();
        return true;
    }

    bool ScreenAuthoringSession::IsLoaded() const noexcept
    {
        return loaded_;
    }

    bool ScreenAuthoringSession::IsDirty() const noexcept
    {
        return loaded_ &&
            (savedHistoryIndex_ == InvalidHistoryIndex ||
             savedHistoryIndex_ != historyIndex_);
    }

    bool ScreenAuthoringSession::CanUndo() const noexcept
    {
        return loaded_ && historyIndex_ > 0;
    }

    bool ScreenAuthoringSession::CanRedo() const noexcept
    {
        return loaded_ && historyIndex_ + 1 < history_.size();
    }

    std::size_t ScreenAuthoringSession::UndoCount() const noexcept
    {
        return CanUndo() ? historyIndex_ : 0;
    }

    std::size_t ScreenAuthoringSession::RedoCount() const noexcept
    {
        return CanRedo() ? history_.size() - historyIndex_ - 1 : 0;
    }

    const ScreenDocument& ScreenAuthoringSession::Document() const noexcept
    {
        return loaded_ && historyIndex_ < history_.size()
            ? history_[historyIndex_] : emptyDocument_;
    }

    const ScreenWidget* ScreenAuthoringSession::FindWidget(
        const StableId& widgetId) const noexcept
    {
        if (!loaded_) return nullptr;
        const auto& widgets = Document().widgets;
        const auto found = std::find_if(
            widgets.begin(), widgets.end(),
            [&widgetId](const ScreenWidget& widget)
            {
                return widget.id == widgetId;
            });
        return found == widgets.end() ? nullptr : &*found;
    }

    const std::string& ScreenAuthoringSession::FilePath() const noexcept
    {
        return filePath_;
    }

    const StableId& ScreenAuthoringSession::ProjectId() const noexcept
    {
        return projectId_;
    }

    bool ScreenAuthoringSession::Adopt(
        ScreenDocument document,
        const StableId& expectedProjectId,
        std::string filePath,
        std::string& error)
    {
        if (!ValidateScreenDocument(document, expectedProjectId, error))
            return false;
        if (filePath.empty())
        {
            error = "Screen authoring requires a document path.";
            return false;
        }
        history_.clear();
        history_.push_back(std::move(document));
        historyIndex_ = 0;
        savedHistoryIndex_ = 0;
        projectId_ = expectedProjectId;
        filePath_ = std::move(filePath);
        loaded_ = true;
        error.clear();
        return true;
    }

    bool ScreenAuthoringSession::CommitMutation(
        ScreenDocument document,
        std::string& error)
    {
        if (!loaded_)
        {
            error = "Screen authoring session is not open.";
            return false;
        }
        if (!ValidateScreenDocument(document, projectId_, error))
            return false;

        if (historyIndex_ + 1 < history_.size())
        {
            if (savedHistoryIndex_ != InvalidHistoryIndex &&
                savedHistoryIndex_ > historyIndex_)
                savedHistoryIndex_ = InvalidHistoryIndex;
            history_.erase(
                history_.begin() +
                    static_cast<std::ptrdiff_t>(historyIndex_ + 1),
                history_.end());
        }

        history_.push_back(std::move(document));
        historyIndex_ = history_.size() - 1;
        if (history_.size() > MaximumHistoryEntries)
        {
            history_.erase(history_.begin());
            --historyIndex_;
            if (savedHistoryIndex_ != InvalidHistoryIndex)
            {
                if (savedHistoryIndex_ == 0)
                    savedHistoryIndex_ = InvalidHistoryIndex;
                else
                    --savedHistoryIndex_;
            }
        }
        error.clear();
        return true;
    }
}
