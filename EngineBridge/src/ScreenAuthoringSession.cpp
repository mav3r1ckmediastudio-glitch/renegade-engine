#include "renegade/bridge/ScreenAuthoringSession.h"

#include <algorithm>
#include <cmath>
#include <iterator>
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

    std::string UniqueCopyName(
        const renegade::bridge::ScreenDocument& document,
        const std::string& sourceName)
    {
        const std::string base = sourceName + " Copy";
        const auto nameExists = [&document](const std::string& name)
        {
            return std::any_of(
                document.widgets.begin(), document.widgets.end(),
                [&name](const renegade::bridge::ScreenWidget& widget)
                {
                    return widget.name == name;
                });
        };
        if (!nameExists(base)) return base;
        for (std::size_t suffix = 2; suffix < 10000; ++suffix)
        {
            const std::string candidate =
                base + " " + std::to_string(suffix);
            if (!nameExists(candidate)) return candidate;
        }
        return base + " Unique";
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

    bool ScreenAuthoringSession::CreateWidget(
        ScreenWidget widget,
        const bool insertAtBack,
        StableId& createdWidgetId,
        std::string& error)
    {
        createdWidgetId.clear();
        if (!loaded_)
        {
            error = "Screen authoring session is not open.";
            return false;
        }

        ScreenDocument candidate = Document();
        if (widget.id.empty()) widget.id = GenerateStableId();
        if (!IsValidStableId(widget.id))
        {
            error = "New Screen widget did not provide a valid stable ID.";
            return false;
        }
        if (std::any_of(
                candidate.widgets.begin(), candidate.widgets.end(),
                [&widget](const ScreenWidget& existing)
                {
                    return existing.id == widget.id;
                }))
        {
            error = "New Screen widget stable ID already exists.";
            return false;
        }

        const StableId newId = widget.id;
        if (insertAtBack)
            candidate.widgets.insert(candidate.widgets.begin(), std::move(widget));
        else
            candidate.widgets.push_back(std::move(widget));

        const auto* created = [&candidate, &newId]() -> const ScreenWidget*
        {
            const auto found = std::find_if(
                candidate.widgets.begin(), candidate.widgets.end(),
                [&newId](const ScreenWidget& value)
                {
                    return value.id == newId;
                });
            return found == candidate.widgets.end() ? nullptr : &*found;
        }();
        if (created && created->kind == ScreenWidgetKind::Button)
            candidate.focusOrder.push_back(newId);

        if (!CommitMutation(std::move(candidate), error)) return false;
        createdWidgetId = newId;
        return true;
    }

    bool ScreenAuthoringSession::DuplicateWidget(
        const StableId& widgetId,
        StableId& duplicatedWidgetId,
        std::string& error)
    {
        duplicatedWidgetId.clear();
        if (!loaded_)
        {
            error = "Screen authoring session is not open.";
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

        ScreenWidget duplicate = *found;
        duplicate.id = GenerateStableId();
        duplicate.name = UniqueCopyName(candidate, found->name);
        const StableId duplicateId = duplicate.id;
        const bool isButton = duplicate.kind == ScreenWidgetKind::Button;
        const auto insertPosition =
            candidate.widgets.begin() +
            static_cast<std::ptrdiff_t>(
                std::distance(candidate.widgets.begin(), found) + 1);
        candidate.widgets.insert(insertPosition, std::move(duplicate));

        if (isButton)
        {
            const auto focus = std::find(
                candidate.focusOrder.begin(), candidate.focusOrder.end(), widgetId);
            if (focus == candidate.focusOrder.end())
            {
                error = "Screen button is missing from focus order.";
                return false;
            }
            candidate.focusOrder.insert(focus + 1, duplicateId);
        }

        if (!CommitMutation(std::move(candidate), error)) return false;
        duplicatedWidgetId = duplicateId;
        return true;
    }

    bool ScreenAuthoringSession::DeleteWidget(
        const StableId& widgetId,
        std::string& error)
    {
        if (!loaded_)
        {
            error = "Screen authoring session is not open.";
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
        if (std::any_of(
                candidate.widgets.begin(), candidate.widgets.end(),
                [&widgetId](const ScreenWidget& widget)
                {
                    return widget.parentId == widgetId;
                }))
        {
            error = "Screen widget has child elements; delete or reparent them first.";
            return false;
        }

        const bool wasButton = found->kind == ScreenWidgetKind::Button;
        candidate.widgets.erase(found);
        if (wasButton)
        {
            candidate.focusOrder.erase(
                std::remove(
                    candidate.focusOrder.begin(), candidate.focusOrder.end(),
                    widgetId),
                candidate.focusOrder.end());
        }
        return CommitMutation(std::move(candidate), error);
    }

    bool ScreenAuthoringSession::MoveWidgetToBack(
        const StableId& widgetId,
        std::string& error)
    {
        if (!loaded_)
        {
            error = "Screen authoring session is not open.";
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
        if (found == candidate.widgets.begin())
        {
            error.clear();
            return true;
        }
        ScreenWidget widget = std::move(*found);
        candidate.widgets.erase(found);
        candidate.widgets.insert(candidate.widgets.begin(), std::move(widget));
        return CommitMutation(std::move(candidate), error);
    }

    bool ScreenAuthoringSession::MoveWidgetToFront(
        const StableId& widgetId,
        std::string& error)
    {
        if (!loaded_)
        {
            error = "Screen authoring session is not open.";
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
        if (std::next(found) == candidate.widgets.end())
        {
            error.clear();
            return true;
        }
        ScreenWidget widget = std::move(*found);
        candidate.widgets.erase(found);
        candidate.widgets.push_back(std::move(widget));
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
