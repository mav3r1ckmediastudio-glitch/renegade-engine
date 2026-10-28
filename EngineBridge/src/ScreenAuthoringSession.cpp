#include "renegade/bridge/ScreenAuthoringSession.h"
#include "renegade/bridge/InventoryPagedScreenAdapter.h"
#include "renegade/bridge/ScreenThemeResolver.h"
#include "renegade/bridge/AuthoringDocumentSnapshot.h"

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

    // Use governed symbolic action names. One Button owns one outcome unless
    // a creator explicitly chooses to share outcomes in StoryFlow.
    std::string NextButtonActionId(
        const renegade::bridge::ScreenDocument& document)
    {
        if (document.actions.size() >= renegade::bridge::ScreenDocument::MaximumActions) return {};
        for (std::size_t suffix = 1; suffix <= renegade::bridge::ScreenDocument::MaximumActions; ++suffix)
        {
            const std::string candidate =
                "ui_button_" + std::to_string(suffix);
            const bool taken = std::any_of(document.actions.begin(),
                document.actions.end(),
                [&candidate](const renegade::bridge::ScreenAction& action)
                {
                    return action.id == candidate;
                });
            if (!taken) return candidate;
        }
        return {};
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
        std::vector<std::uint8_t> original;
        if (!ReadAuthoringDocumentSnapshot(filePath, original, error)) return false;
        ScreenDocument document;
        if (!ReadScreenDocument(filePath, expectedProjectId, document, error) ||
            !MatchesAuthoringDocumentSnapshot(filePath, original, error))
            return false;
        if (!Adopt(std::move(document), expectedProjectId, filePath, error))
            return false;
        persistedBytes_ = std::move(original);
        return true;
    }

    bool ScreenAuthoringSession::Reload(std::string& error)
    {
        if (!loaded_ || filePath_.empty())
        {
            error = "Authoring session is not open.";
            return false;
        }
        return Open(filePath_, projectId_, error);
    }

    bool ScreenAuthoringSession::Save(std::string& error)
    {
        if (!loaded_ || filePath_.empty())
        {
            error = "Screen authoring session is not open.";
            return false;
        }
        if (!MatchesAuthoringDocumentSnapshot(filePath_, persistedBytes_, error))
            return false;
        if (!WriteScreenDocument(filePath_, Document(), error))
            return false;
        std::vector<std::uint8_t> committed;
        if (!ReadAuthoringDocumentSnapshot(filePath_, committed, error)) return false;
        persistedBytes_ = std::move(committed);
        savedHistoryIndex_ = historyIndex_;
        persistedSnapshot_ = Document();
        error.clear();
        return true;
    }

    void ScreenAuthoringSession::Clear() noexcept
    {
        ++revision_;
        history_.clear();
        persistedSnapshot_ = {};
        persistedBytes_.clear();
        historyIndex_ = 0;
        savedHistoryIndex_ = InvalidHistoryIndex;
        projectId_.clear();
        filePath_.clear();
        loaded_ = false;
        coalescedEditActive_ = false;
        coalescedMutationCreated_ = false;
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

        if ((edit.fill || edit.fillDirection) && found->kind != ScreenWidgetKind::Progress)
        { error = "Fill editing requires a Progress element."; return false; }
        const bool same = found->name == edit.name &&
            found->text == edit.text && found->visible == edit.visible &&
            found->enabled == edit.enabled &&
            (!edit.fill || found->fill == *edit.fill) &&
            (!edit.fillDirection || found->fillDirection == *edit.fillDirection) &&
            SameRect(currentResolved, edit.resolvedRect);
        if (same)
        {
            error.clear();
            return true;
        }

        const auto* parent=FindWidget(found->parentId);
        if(parent&&EffectiveScreenChildLayout(parent->container,Document().designWidth)!=ScreenChildLayout::Free&&
            (std::abs(currentResolved.x-edit.resolvedRect.x)>0.051f||
             std::abs(currentResolved.y-edit.resolvedRect.y)>0.051f||
             std::abs(currentResolved.width-edit.resolvedRect.width)>0.051f||
             std::abs(currentResolved.height-edit.resolvedRect.height)>0.051f))
        {error="Geometry is managed by the parent container. Edit the parent layout or reparent this element.";return false;}
        if(found->component)
        {
            if(found->text!=edit.text)found->component->overrides|=64;
            if((edit.fill&&*edit.fill!=found->fill)||(edit.fillDirection&&*edit.fillDirection!=found->fillDirection))
                found->component->overrides|=512;
        }
        found->name = std::move(edit.name);
        found->text = std::move(edit.text);
        found->visible = edit.visible;
        found->enabled = edit.enabled;
        if (edit.fill) found->fill = *edit.fill;
        if (edit.fillDirection) found->fillDirection = *edit.fillDirection;

        if(parent&&EffectiveScreenChildLayout(parent->container,Document().designWidth)!=ScreenChildLayout::Free)
            return CommitMutation(std::move(candidate),error);
        ScreenRect parentRect{0.0f, 0.0f,
  candidate.designWidth, candidate.designHeight};
        if (!found->parentId.empty() &&
  !ResolveScreenWidgetRect(candidate, found->parentId, parentRect, error))
  return false;
        if(found->parentId.empty()&&found->layoutMode==ScreenLayoutMode::Anchored)
        {
            parentRect.x=candidate.safeArea.left;
            parentRect.y=candidate.safeArea.top;
            parentRect.width-=candidate.safeArea.left+candidate.safeArea.right;
            parentRect.height-=candidate.safeArea.top+candidate.safeArea.bottom;
        }
        const float relativeX = edit.resolvedRect.x - parentRect.x;
        const float relativeY = edit.resolvedRect.y - parentRect.y;
        if (found->layoutMode == ScreenLayoutMode::Absolute)
  found->rect = {relativeX, relativeY, edit.resolvedRect.width, edit.resolvedRect.height};
        else
        {
  found->anchors.offsetMinimumX = relativeX - found->anchors.minimumX * parentRect.width;
  found->anchors.offsetMinimumY = relativeY - found->anchors.minimumY * parentRect.height;
  found->anchors.offsetMaximumX = relativeX + edit.resolvedRect.width - found->anchors.maximumX * parentRect.width;
  found->anchors.offsetMaximumY = relativeY + edit.resolvedRect.height - found->anchors.maximumY * parentRect.height;
        }

        return CommitMutation(std::move(candidate), error);
    }

    bool ScreenAuthoringSession::CreateButtonWithUniqueAction(
        ScreenWidget widget,
        StableId& createdWidgetId,
        std::string& error)
    {
        createdWidgetId.clear();
        if (!loaded_)
        {
            error = "Screen authoring session is not open.";
            return false;
        }
        if (widget.kind != ScreenWidgetKind::Button)
        {
            error = "Only Buttons may request an owned Screen action.";
            return false;
        }
        ScreenDocument candidate = Document();
        const std::string actionId = NextButtonActionId(candidate);
        if (actionId.empty())
        {
            error = "This Screen already has the maximum of 128 actions.";
            return false;
        }
        if (widget.id.empty()) widget.id = GenerateStableId();
        if (!IsValidStableId(widget.id) ||
            std::any_of(candidate.widgets.begin(), candidate.widgets.end(),
                [&widget](const ScreenWidget& current)
                {
                    return current.id == widget.id;
                }))
        {
            error = "New Button requires an unused valid stable ID.";
            return false;
        }
        widget.actionId = actionId;
        const StableId created = widget.id;
        candidate.actions.push_back({actionId});
        candidate.widgets.push_back(std::move(widget));
        candidate.focusOrder.push_back(created);
        if (!CommitMutation(std::move(candidate), error)) return false;
        createdWidgetId = created;
        return true;
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
        if ((widget.kind == ScreenWidgetKind::Progress || IsScreenValueControl(widget.kind) || !widget.bindings.empty() || !widget.animations.empty() || widget.container.mode != ScreenChildLayout::Free || widget.container.compactBelowWidth>0) && candidate.schemaVersion < 3)
        {
            candidate.migratedFromVersion = candidate.schemaVersion;
            candidate.schemaVersion = 3;
        }
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
        if (created && IsScreenInteractive(created->kind))
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
        if (isButton)
        {
            const std::string newActionId = NextButtonActionId(candidate);
            if (newActionId.empty())
            {
                error = "Cannot duplicate Button: Screen action limit reached.";
                return false;
            }
            duplicate.actionId = newActionId;
            candidate.actions.push_back({newActionId});
        }
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
        if(found->component&&!found->component->treeRootId.empty())
        {error="Delete the complete linked tree root or detach its children first.";return false;}
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

    bool ScreenAuthoringSession::MoveWidgetLayer(
        const StableId& widgetId,
        const int direction,
        const bool toBoundary,
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

        // Rearrange only sibling elements. Unrelated parent containers must
        // not have their own internal ordering changed by a layer command.
        const StableId parentId = found->parentId;
        std::vector<std::size_t> siblingPositions;
        siblingPositions.reserve(candidate.widgets.size());
        std::size_t currentSibling = 0;
        for (std::size_t i = 0; i < candidate.widgets.size(); ++i)
        {
            if (candidate.widgets[i].parentId != parentId) continue;
            if (candidate.widgets[i].id == widgetId)
                currentSibling = siblingPositions.size();
            siblingPositions.push_back(i);
        }

        if (siblingPositions.size() < 2 ||
            (direction < 0 && currentSibling == 0) ||
            (direction > 0 && currentSibling + 1 == siblingPositions.size()))
        {
            error.clear();
            return true;
        }

        const std::size_t targetSibling = toBoundary
            ? (direction < 0 ? 0 : siblingPositions.size() - 1)
            : (direction < 0 ? currentSibling - 1 : currentSibling + 1);
        if (direction < 0)
        {
            for (std::size_t i = currentSibling; i > targetSibling; --i)
            {
                std::swap(candidate.widgets[siblingPositions[i]],
                    candidate.widgets[siblingPositions[i - 1]]);
            }
        }
        else
        {
            for (std::size_t i = currentSibling; i < targetSibling; ++i)
            {
                std::swap(candidate.widgets[siblingPositions[i]],
                    candidate.widgets[siblingPositions[i + 1]]);
            }
        }
        return CommitMutation(std::move(candidate), error);
    }

    bool ScreenAuthoringSession::MoveWidgetToBack(
        const StableId& widgetId, std::string& error)
    {
        return MoveWidgetLayer(widgetId, -1, true, error);
    }

    bool ScreenAuthoringSession::MoveWidgetBackward(
        const StableId& widgetId, std::string& error)
    {
        return MoveWidgetLayer(widgetId, -1, false, error);
    }

    bool ScreenAuthoringSession::MoveWidgetForward(
        const StableId& widgetId, std::string& error)
    {
        return MoveWidgetLayer(widgetId, 1, false, error);
    }

    bool ScreenAuthoringSession::MoveWidgetToFront(
        const StableId& widgetId, std::string& error)
    {
        return MoveWidgetLayer(widgetId, 1, true, error);
    }

    bool ScreenAuthoringSession::MoveWidgetAdjacentTo(
        const StableId& widgetId,
        const StableId& targetSiblingId,
        const bool placeAfter,
        std::string& error)
    {
        if (!loaded_)
        {
            error = "Screen authoring session is not open.";
            return false;
        }
        ScreenDocument candidate = Document();
        const auto find = [&](const StableId& id)
        {
            return std::find_if(candidate.widgets.begin(), candidate.widgets.end(),
                [&id](const ScreenWidget& widget) { return widget.id == id; });
        };
        const auto source = find(widgetId);
        const auto destination = find(targetSiblingId);
        if (source == candidate.widgets.end() ||
            destination == candidate.widgets.end())
        {
            error = "Layer drag source or destination no longer exists.";
            return false;
        }
        if (source->parentId != destination->parentId)
        {
            error = "Layer drag must stay within the same parent container.";
            return false;
        }
        if (widgetId == targetSiblingId)
        {
            error.clear();
            return true;
        }

        std::vector<std::size_t> positions;
        std::vector<ScreenWidget> siblings;
        for (std::size_t i = 0; i < candidate.widgets.size(); ++i)
        {
            if (candidate.widgets[i].parentId != source->parentId) continue;
            positions.push_back(i);
            siblings.push_back(candidate.widgets[i]);
        }
        const auto current = std::find_if(siblings.begin(), siblings.end(),
            [&](const ScreenWidget& value) { return value.id == widgetId; });
        const auto target = std::find_if(siblings.begin(), siblings.end(),
            [&](const ScreenWidget& value) { return value.id == targetSiblingId; });
        const std::size_t from = static_cast<std::size_t>(
            std::distance(siblings.begin(), current));
        std::size_t insert = static_cast<std::size_t>(
            std::distance(siblings.begin(), target)) + (placeAfter ? 1 : 0);
        if (insert > from) --insert;
        if (insert == from)
        {
            error.clear();
            return true; // No redundant history entry for a no-op drop.
        }
        ScreenWidget moved = std::move(siblings[from]);
        siblings.erase(siblings.begin() + static_cast<std::ptrdiff_t>(from));
        siblings.insert(siblings.begin() + static_cast<std::ptrdiff_t>(insert),
            std::move(moved));
        for (std::size_t i = 0; i < siblings.size(); ++i)
            candidate.widgets[positions[i]] = std::move(siblings[i]);
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
        ++revision_;
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
        ++revision_;
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
        ++revision_;
        history_.clear();
        history_.push_back(std::move(document));
        persistedSnapshot_ = history_.front();
        historyIndex_ = 0;
        savedHistoryIndex_ = 0;
        projectId_ = expectedProjectId;
        filePath_ = std::move(filePath);
        loaded_ = true;
        coalescedEditActive_ = false;
        coalescedMutationCreated_ = false;
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
        // The user can move an entire linked tree through its instance
        // root, but changing the parent of an individual linked member
        // silently severs its source hierarchy. Reject that authoring edit
        // before it enters Undo or autosave (including grouping operations).
        for(const auto& widget:document.widgets)
        {
            if(!widget.component||widget.component->treeRootId.empty()||
                widget.id==widget.component->treeRootId)continue;
            const auto* previous=FindWidget(widget.id);
            if(previous&&previous->component&&
                previous->component->treeRootId==widget.component->treeRootId&&
                previous->parentId!=widget.parentId)
            {error="Linked hierarchy member cannot be reparented; detach the complete instance first.";return false;}
        }
        if (!ValidateScreenDocument(document, projectId_, error))
            return false;

        ++revision_;
        if (coalescedEditActive_ && coalescedMutationCreated_)
        {
            history_[historyIndex_] = std::move(document);
            error.clear();
            return true;
        }

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
        ++historyMutation_;
        historyIndex_ = history_.size() - 1;
        if (coalescedEditActive_) coalescedMutationCreated_ = true;
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


// Batch operations share the same validated candidate/history boundary.
namespace renegade::bridge
{
    namespace
    {
        // The shared renderer and native hit tests consume this flat paint list.
        // Keep each container subtree contiguous in sibling order after group edits.
        void OrderWidgetSubtrees(ScreenDocument& doc)
        {
            std::vector<ScreenWidget> ordered;
            ordered.reserve(doc.widgets.size());
            const auto visit=[&](const auto& self,const StableId& parent)->void
            {
                for(const auto& w:doc.widgets)if(w.parentId==parent)
                {ordered.push_back(w);self(self,w.id);}
            };
            visit(visit,StableId{});
            if(ordered.size()==doc.widgets.size())doc.widgets=std::move(ordered);
        }
        bool SelectedRoots(const ScreenDocument& doc, const std::vector<StableId>& ids,
            std::unordered_set<StableId>& selected, std::unordered_set<StableId>& roots,
            std::string& error)
        {
            if (ids.empty()) { error = "Select at least one element."; return false; }
            for (const auto& id : ids)
            {
                if (std::none_of(doc.widgets.begin(),doc.widgets.end(),
                    [&](const auto& w){return w.id==id;}))
                { error="Selected element no longer exists."; return false; }
                selected.insert(id);
            }
            for (const auto& w : doc.widgets)
            {
                if (!selected.count(w.id)) continue;
                auto parent=w.parentId;
                bool nested=false;
                std::unordered_set<StableId> visited;
                while (!parent.empty())
                {
                    if (!visited.insert(parent).second) { error="Cyclic parent graph.";return false; }
                    if (selected.count(parent)) {nested=true;break;}
                    const auto p=std::find_if(doc.widgets.begin(),doc.widgets.end(),
                        [&](const auto& item){return item.id==parent;});
                    if(p==doc.widgets.end()){error="Missing parent.";return false;}
                    parent=p->parentId;
                }
                if(!nested)roots.insert(w.id);
            }
            return true;
        }
        void ExpandDescendants(const ScreenDocument& doc,std::unordered_set<StableId>& ids)
        {
            bool more=true;
            while(more){more=false;for(const auto& w:doc.widgets)
                if(ids.count(w.parentId)&&ids.insert(w.id).second)more=true;}
        }
        bool SetResolvedGeometry(ScreenDocument& doc, ScreenWidget& w, const ScreenRect& rect,
            std::string& error)
        {
            ScreenRect p{0,0,doc.designWidth,doc.designHeight};
            if(!w.parentId.empty()&&!ResolveScreenWidgetRect(doc,w.parentId,p,error))return false;
            if(w.parentId.empty()&&w.layoutMode==ScreenLayoutMode::Anchored)
            {
                p.x=doc.safeArea.left;p.y=doc.safeArea.top;
                p.width-=doc.safeArea.left+doc.safeArea.right;
                p.height-=doc.safeArea.top+doc.safeArea.bottom;
            }
            if(w.layoutMode==ScreenLayoutMode::Absolute)
                w.rect={rect.x-p.x,rect.y-p.y,rect.width,rect.height};
            else
            {
                w.anchors.offsetMinimumX=rect.x-p.x-w.anchors.minimumX*p.width;
                w.anchors.offsetMinimumY=rect.y-p.y-w.anchors.minimumY*p.height;
                w.anchors.offsetMaximumX=rect.x-p.x+rect.width-w.anchors.maximumX*p.width;
                w.anchors.offsetMaximumY=rect.y-p.y+rect.height-w.anchors.maximumY*p.height;
            }
            return true;
        }
    }
    bool ScreenAuthoringSession::CopyWidgets(const std::vector<StableId>& ids,
        ScreenWidgetClipboard& clipboard,std::string& error) const
    {
        if(!loaded_){error="No Screen is open.";return false;}
        std::unordered_set<StableId> all,roots;
        if(!SelectedRoots(Document(),ids,all,roots,error))return false;
        all=roots;ExpandDescendants(Document(),all);
        ScreenWidgetClipboard result;result.projectId=projectId_;
        auto copyDocument=Document();
        if(copyDocument.theme||std::any_of(copyDocument.widgets.begin(),copyDocument.widgets.end(),[](const auto& w){return w.component.has_value();}))
        {
            auto content=std::filesystem::u8path(filePath_).parent_path();
            while(!content.empty()&&content.filename()!="Content"&&content!=content.parent_path())
                content=content.parent_path();
            if(content.filename()!="Content"){error="Cannot resolve project theme outside Content.";return false;}
            ScreenDocument effective;
            if(!ResolveScreenTheme(content.parent_path().generic_u8string(),copyDocument,effective,error))return false;
            copyDocument=std::move(effective);
        }
        for(const auto& source:copyDocument.widgets)
        {
            if(!all.count(source.id))continue;
            auto w=source;w.themeOverrides=63;w.component.reset();
            if(roots.count(w.id))
            {
                if(!ResolveScreenWidgetRect(Document(),w.id,w.rect,error))return false;
                w.parentId.clear();w.layoutMode=ScreenLayoutMode::Absolute;
            }
            result.widgets.push_back(std::move(w));
        }
        for(const auto& id:Document().focusOrder)if(all.count(id))result.focusOrder.push_back(id);
        clipboard=std::move(result);error.clear();return true;
    }
    bool ScreenAuthoringSession::PasteWidgets(const ScreenWidgetClipboard& clipboard,
        float dx,float dy,std::vector<StableId>& createdRoots,std::string& error)
    {
        createdRoots.clear();
        if(!loaded_||clipboard.projectId!=projectId_)
        {error="Clipboard belongs to another project. Adopt its resources before copying.";return false;}
        if(clipboard.widgets.empty()||!std::isfinite(dx)||!std::isfinite(dy))
        {error="Clipboard is empty or paste offset is invalid.";return false;}
        auto candidate=Document();
        if(candidate.schemaVersion<3&&std::any_of(clipboard.widgets.begin(),clipboard.widgets.end(),
            [](const auto& w){return w.kind==ScreenWidgetKind::Progress||IsScreenValueControl(w.kind)||!w.bindings.empty()||!w.animations.empty()||w.container.mode!=ScreenChildLayout::Free||w.container.compactBelowWidth>0;}))
        {candidate.migratedFromVersion=candidate.schemaVersion;candidate.schemaVersion=3;}
        if(candidate.widgets.size()+clipboard.widgets.size()>256)
        {error="Paste exceeds the Screen element limit.";return false;}
        std::unordered_map<StableId,StableId> remap;
        for(const auto& w:clipboard.widgets)
            if(w.id.empty()||!remap.emplace(w.id,GenerateStableId()).second)
            {error="Clipboard contains invalid or duplicate IDs.";return false;}
        std::vector<StableId> roots;
        for(const auto& source:clipboard.widgets)
        {
            auto w=source;w.id=remap.at(source.id);
            for(auto& action:w.interactions)
            {
                const auto target=remap.find(action.targetId);if(target!=remap.end())action.targetId=target->second;
                if(action.condition){const auto source=remap.find(action.condition->widgetId);
                    if(source!=remap.end())action.condition->widgetId=source->second;}
            }
            if(source.parentId.empty()){w.rect.x+=dx;w.rect.y+=dy;roots.push_back(w.id);}
            else
            {
                const auto p=remap.find(source.parentId);
                if(p==remap.end()){error="Clipboard has an external parent.";return false;}
                w.parentId=p->second;
            }
            w.name=UniqueCopyName(candidate,source.name);
            if(w.kind==ScreenWidgetKind::Button)
            {
                w.actionId=NextButtonActionId(candidate);
                if(w.actionId.empty()){error="Paste exceeds the Screen action limit.";return false;}
                candidate.actions.push_back({w.actionId});
            }
            candidate.widgets.push_back(std::move(w));
        }
        for(const auto& id:clipboard.focusOrder)
        {
            const auto p=remap.find(id);
            if(p==remap.end()){error="Clipboard focus entry has no element.";return false;}
            candidate.focusOrder.push_back(p->second);
        }
        if(!CommitMutation(std::move(candidate),error))return false;
        createdRoots=std::move(roots);return true;
    }
    bool ScreenAuthoringSession::DeleteWidgets(const std::vector<StableId>& ids,std::string& error)
    {
        if(!loaded_){error="No Screen is open.";return false;}
        std::unordered_set<StableId> all,roots;
        if(!SelectedRoots(Document(),ids,all,roots,error))return false;
        all=roots;ExpandDescendants(Document(),all);
        for(const auto& widget:Document().widgets)
            if(all.count(widget.id)&&widget.component&&
                !widget.component->treeRootId.empty()&&
                !all.count(widget.component->treeRootId))
            {error="Delete the full linked hierarchy or detach it before removing members.";return false;}
        auto c=Document();
        c.widgets.erase(std::remove_if(c.widgets.begin(),c.widgets.end(),
            [&](const auto& w){return all.count(w.id)!=0;}),c.widgets.end());
        c.focusOrder.erase(std::remove_if(c.focusOrder.begin(),c.focusOrder.end(),
            [&](const auto& id){return all.count(id)!=0;}),c.focusOrder.end());
        return CommitMutation(std::move(c),error);
    }
    bool ScreenAuthoringSession::TranslateWidgets(const std::vector<StableId>& ids,
        float dx,float dy,std::string& error)
    {
        if(!loaded_||!std::isfinite(dx)||!std::isfinite(dy))
        {error="Invalid element movement.";return false;}
        std::unordered_set<StableId> selected,roots;
        if(!SelectedRoots(Document(),ids,selected,roots,error))return false;
        for(const auto& id:roots)
        {
            const auto* parent=FindWidget(FindWidget(id)->parentId);
            if(parent&&EffectiveScreenChildLayout(parent->container,Document().designWidth)!=ScreenChildLayout::Free)
            {error="Move the container or reorder its children; their positions are layout-managed.";return false;}
        }
        if(dx==0&&dy==0){error.clear();return true;}
        auto c=Document();
        for(auto& w:c.widgets)if(roots.count(w.id))
        {
            if(w.layoutMode==ScreenLayoutMode::Absolute){w.rect.x+=dx;w.rect.y+=dy;}
            else{w.anchors.offsetMinimumX+=dx;w.anchors.offsetMaximumX+=dx;
                w.anchors.offsetMinimumY+=dy;w.anchors.offsetMaximumY+=dy;}
        }
        return CommitMutation(std::move(c),error);
    }
    bool ScreenAuthoringSession::MoveWidgetsLayer(const std::vector<StableId>& ids,
        int direction,bool boundary,std::string& error)
    {
        if(!loaded_||(direction!=-1&&direction!=1)){error="Invalid layer move.";return false;}
        std::unordered_set<StableId> selected,roots;
        if(!SelectedRoots(Document(),ids,selected,roots,error))return false;
        auto c=Document();std::unordered_set<StableId> parents;
        for(const auto& w:c.widgets)if(roots.count(w.id))parents.insert(w.parentId);
        bool changed=false;
        for(const auto& parent:parents)
        {
            std::vector<std::size_t> slots;std::vector<ScreenWidget> siblings;
            for(std::size_t i=0;i<c.widgets.size();++i)if(c.widgets[i].parentId==parent)
            {slots.push_back(i);siblings.push_back(c.widgets[i]);}
            const auto original=siblings;
            const auto chosen=[&](const ScreenWidget& w){return roots.count(w.id)!=0;};
            if(boundary)
                std::stable_partition(siblings.begin(),siblings.end(),
                    [&](const auto& w){return direction<0?chosen(w):!chosen(w);});
            else if(direction>0)
            {for(std::size_t i=siblings.size();i>1;--i)
                if(chosen(siblings[i-2])&&!chosen(siblings[i-1]))std::swap(siblings[i-2],siblings[i-1]);}
            else
            {for(std::size_t i=1;i<siblings.size();++i)
                if(chosen(siblings[i])&&!chosen(siblings[i-1]))std::swap(siblings[i],siblings[i-1]);}
            for(std::size_t i=0;i<slots.size();++i)
            {changed=changed||original[i].id!=siblings[i].id;c.widgets[slots[i]]=std::move(siblings[i]);}
        }
        if(!changed){error.clear();return true;}
        OrderWidgetSubtrees(c);
        return CommitMutation(std::move(c),error);
    }
    bool ScreenAuthoringSession::MoveWidgetsAdjacentTo(const std::vector<StableId>& ids,
        const StableId& target,bool after,std::string& error)
    {
        if(!loaded_){error="No Screen is open.";return false;}
        std::unordered_set<StableId> selected,roots;
        if(!SelectedRoots(Document(),ids,selected,roots,error))return false;
        const auto* dest=FindWidget(target);
        if(!dest||roots.count(target)){error="Choose an unselected sibling.";return false;}
        for(const auto& id:roots)if(FindWidget(id)->parentId!=dest->parentId)
        {error="Layer drag requires elements with the same parent.";return false;}
        auto c=Document();std::vector<std::size_t> slots;
        std::vector<ScreenWidget> kept,moving;
        for(std::size_t i=0;i<c.widgets.size();++i)if(c.widgets[i].parentId==dest->parentId)
        {slots.push_back(i);if(roots.count(c.widgets[i].id))moving.push_back(c.widgets[i]);
            else kept.push_back(c.widgets[i]);}
        auto pos=std::find_if(kept.begin(),kept.end(),[&](const auto& w){return w.id==target;});
        if(after)++pos;
        kept.insert(pos,moving.begin(),moving.end());
        bool changed=false;
        for(std::size_t i=0;i<slots.size();++i)
        {changed=changed||c.widgets[slots[i]].id!=kept[i].id;c.widgets[slots[i]]=std::move(kept[i]);}
        if(!changed){error.clear();return true;}
        OrderWidgetSubtrees(c);
        return CommitMutation(std::move(c),error);
    }
    bool ScreenAuthoringSession::GroupWidgets(const std::vector<StableId>& ids,
        StableId& groupId,std::string& error)
    {
        groupId.clear();
        if(!loaded_){error="No Screen is open.";return false;}
        std::unordered_set<StableId> selected,roots;
        if(!SelectedRoots(Document(),ids,selected,roots,error))return false;
        if(roots.size()<2){error="Select at least two sibling elements.";return false;}
        const auto parent=FindWidget(*roots.begin())->parentId;
        const auto* owner=FindWidget(parent);
        if(owner&&EffectiveScreenChildLayout(owner->container,Document().designWidth)!=ScreenChildLayout::Free)
        {error="Reparent layout-managed elements before grouping them.";return false;}
        std::unordered_map<StableId,ScreenRect> geometry;
        ScreenRect bounds{};bool first=true;
        for(const auto& w:Document().widgets)if(roots.count(w.id))
        {
            if(w.parentId!=parent){error="Grouping requires a shared parent.";return false;}
            ScreenRect rect;
            if(!ResolveScreenWidgetRect(Document(),w.id,rect,error))return false;
            geometry.emplace(w.id,rect);
            if(first){bounds=rect;first=false;}else
            {const float right=std::max(bounds.x+bounds.width,rect.x+rect.width);
             const float bottom=std::max(bounds.y+bounds.height,rect.y+rect.height);
             bounds.x=std::min(bounds.x,rect.x);bounds.y=std::min(bounds.y,rect.y);
             bounds.width=right-bounds.x;bounds.height=bottom-bounds.y;}
        }
        auto c=Document();ScreenWidget group;
        group.id=GenerateStableId();group.kind=ScreenWidgetKind::Image;
        group.name="Group";group.parentId=parent;group.enabled=false;
        group.rect=bounds;group.style=MakeScreenWidgetStyleTemplate(group.kind,bounds.height);
        group.style.normal={};group.style.hover={};group.style.pressed={};
        group.style.focused={};group.style.disabled={};
        group.style.borderWidth=0;
        if(!SetResolvedGeometry(c,group,bounds,error))return false;
        const auto id=group.id;
        auto position=std::find_if(c.widgets.begin(),c.widgets.end(),
            [&](const auto& w){return roots.count(w.id)!=0;});
        c.widgets.insert(position,group);
        for(auto& w:c.widgets)if(roots.count(w.id))
        {w.parentId=id;if(!SetResolvedGeometry(c,w,geometry.at(w.id),error))return false;}
        OrderWidgetSubtrees(c);
        if(!CommitMutation(std::move(c),error))return false;
        groupId=id;return true;
    }
    bool ScreenAuthoringSession::UngroupWidget(const StableId& groupId,
        std::vector<StableId>& children,std::string& error)
    {
        children.clear();
        if(!loaded_){error="No Screen is open.";return false;}
        const auto* group=FindWidget(groupId);
        if(!group||group->kind!=ScreenWidgetKind::Image||!group->resourcePath.empty())
        {error="Select an image-free container to ungroup.";return false;}
        const auto* owner=FindWidget(group->parentId);
        if(owner&&EffectiveScreenChildLayout(owner->container,Document().designWidth)!=ScreenChildLayout::Free)
        {error="Reparent this layout-managed container before ungrouping.";return false;}
        auto c=Document();std::vector<StableId> result;
        std::unordered_map<StableId,ScreenRect> geometry;
        for(const auto& w:Document().widgets)if(w.parentId==groupId)
        {ScreenRect rect;if(!ResolveScreenWidgetRect(Document(),w.id,rect,error))return false;
            geometry.emplace(w.id,rect);result.push_back(w.id);}
        if(result.empty()){error="Container has no child elements.";return false;}
        const auto parent=group->parentId;
        for(auto& w:c.widgets)if(w.parentId==groupId)
        {w.parentId=parent;if(!SetResolvedGeometry(c,w,geometry.at(w.id),error))return false;}
        c.widgets.erase(std::remove_if(c.widgets.begin(),c.widgets.end(),
            [&](const auto& w){return w.id==groupId;}),c.widgets.end());
        OrderWidgetSubtrees(c);
        if(!CommitMutation(std::move(c),error))return false;
        children=std::move(result);return true;
    }
}

namespace renegade::bridge
{
    bool ScreenAuthoringSession::UpdateWidgetContainer(const StableId& id,
        const ScreenContainerLayout& layout,std::string& error)
    {
        if(!loaded_){error="No Screen is open.";return false;}
        const auto* widget=FindWidget(id);
        if(!widget||widget->kind!=ScreenWidgetKind::Image)
        {error="Select an Image/container to edit child layout.";return false;}
        if(!ValidateScreenContainerLayout(layout,error))return false;
        const auto& old=widget->container;
        if(old.mode==layout.mode&&old.padding==layout.padding&&old.gap==layout.gap&&old.columns==layout.columns&&
            old.compactBelowWidth==layout.compactBelowWidth&&old.compactMode==layout.compactMode&&
            old.compactColumns==layout.compactColumns)
        {error.clear();return true;}
        auto c=Document();
        if(c.schemaVersion<3){c.migratedFromVersion=c.schemaVersion;c.schemaVersion=3;}
        for(auto& w:c.widgets)if(w.id==id){w.container=layout;break;}
        return CommitMutation(std::move(c),error);
    }
}

namespace renegade::bridge
{
    namespace
    {
        bool LoadLinkedTreeSource(const ScreenAuthoringSession& session,
            const ScreenComponentReference& reference, ScreenDocument& source,
            std::string& root, std::string& error)
        {
            auto content=std::filesystem::u8path(session.FilePath()).parent_path();
            while(!content.empty()&&content.filename()!="Content"&&content!=content.parent_path())
                content=content.parent_path();
            if(content.filename()!="Content")
            {error="Linked UI tree requires a project Content root.";return false;}
            root=content.parent_path().generic_u8string();
            if(reference.documentId==session.Document().envelope.documentId)
            {error="Cannot link a Screen hierarchy to itself.";return false;}
            std::string path;
            if(!ResolveRuntimeScreenDocumentPath(root,session.ProjectId(),
                reference.documentId,reference.pathHint,path,error)||
                !ReadScreenDocument(path,session.ProjectId(),source,error))
                return false;
            ScreenDocument effective;
            return ResolveScreenTheme(root,source,effective,error);
        }

        std::unordered_set<StableId> LinkedSourceClosure(
            const ScreenDocument& source,const StableId& rootId)
        {
            std::unordered_set<StableId> ids{rootId};
            bool changed=true;
            while(changed)
            {
                changed=false;
                for(const auto& widget:source.widgets)
                    if(!widget.parentId.empty()&&ids.count(widget.parentId)&&
                        ids.insert(widget.id).second)changed=true;
            }
            return ids;
        }

        bool AppendLinkedTreeMembers(ScreenDocument& candidate,
            const ScreenDocument& source, const ScreenComponentReference& reference,
            const std::unordered_set<StableId>& closure,
            const std::unordered_map<StableId,StableId>& identities,
            const StableId& rootId, std::string& error)
        {
            std::unordered_set<StableId> inserted;
            for(const auto& src:source.widgets)
            {
                if(!closure.count(src.id))continue;
                const auto destId=identities.at(src.id);
                const auto exists=std::find_if(candidate.widgets.begin(),candidate.widgets.end(),
                    [&](const auto& widget){return widget.id==destId;});
                if(exists!=candidate.widgets.end())continue;
                ScreenWidget local=src;
                local.id=destId;
                local.component=ScreenComponentReference{
                    reference.documentId,reference.pathHint,src.id,0,rootId};
                // The instance inherits from its exact source, not an unrelated
                // theme assigned to its destination Screen.
                local.themeOverrides=63;
                if(src.id==reference.widgetId)
                {
                    if(!ResolveScreenWidgetRect(source,src.id,local.rect,error))return false;
                    local.parentId.clear();
                    local.layoutMode=ScreenLayoutMode::Absolute;
                    local.anchors={};
                }
                else
                {
                    const auto parent=identities.find(src.parentId);
                    if(parent==identities.end())
                    {error="Linked subtree has an unmapped source parent.";return false;}
                    local.parentId=parent->second;
                }
                for(auto& action:local.interactions)
                {
                    const auto target=identities.find(action.targetId);
                    if(target==identities.end())
                    {error="Linked subtree interaction targets a widget outside its source hierarchy.";return false;}
                    action.targetId=target->second;
                    if(action.condition)
                    {
                        const auto condition=identities.find(action.condition->widgetId);
                        if(condition==identities.end())
                        {error="Linked subtree condition targets a widget outside its source hierarchy.";return false;}
                        action.condition->widgetId=condition->second;
                    }
                }
                if(local.kind==ScreenWidgetKind::Button)
                {
                    local.actionId=NextButtonActionId(candidate);
                    if(local.actionId.empty())
                    {error="Linked subtree import exceeds the Screen action limit.";return false;}
                    candidate.actions.push_back({local.actionId});
                }
                candidate.widgets.push_back(std::move(local));
                inserted.insert(src.id);
            }
            for(const auto& focusId:source.focusOrder)
            {
                if(inserted.count(focusId))
                    candidate.focusOrder.push_back(identities.at(focusId));
            }
            return true;
        }
    }

    bool ScreenAuthoringSession::ImportLinkedWidgetTree(
        const ScreenComponentReference& reference,StableId& newRoot,std::string& error)
    {
        newRoot.clear();
        if(!loaded_){error="No Screen is open.";return false;}
        if(!reference.treeRootId.empty()||reference.overrides!=0)
        {error="A new linked UI tree must start with inherited values.";return false;}
        ScreenDocument source;std::string root;
        if(!LoadLinkedTreeSource(*this,reference,source,root,error))return false;
        if(std::none_of(source.widgets.begin(),source.widgets.end(),
            [&](const auto& widget){return widget.id==reference.widgetId;}))
        {error="Linked subtree source root no longer exists.";return false;}
        const auto closure=LinkedSourceClosure(source,reference.widgetId);
        auto candidate=Document();
        if(candidate.widgets.size()+closure.size()>256)
        {error="Linked subtree import exceeds the Screen element limit.";return false;}
        if(candidate.schemaVersion<3)
        {candidate.migratedFromVersion=candidate.schemaVersion;candidate.schemaVersion=3;}
        std::unordered_map<StableId,StableId> identities;
        for(const auto& widget:source.widgets)
            if(closure.count(widget.id))identities.emplace(widget.id,GenerateStableId());
        const auto instanceId=identities.at(reference.widgetId);
        if(!AppendLinkedTreeMembers(candidate,source,reference,closure,
            identities,instanceId,error))return false;
        ScreenDocument effective;
        if(!ValidateScreenDocument(candidate,projectId_,error)||
            !ResolveScreenTheme(root,candidate,effective,error)||
            !CommitMutation(std::move(candidate),error))return false;
        newRoot=instanceId;return true;
    }

    bool ScreenAuthoringSession::RefreshLinkedWidgetTree(
        const StableId& instanceRoot,std::string& error)
    {
        if(!loaded_){error="No Screen is open.";return false;}
        const auto* rootWidget=FindWidget(instanceRoot);
        if(!rootWidget||!rootWidget->component||
            rootWidget->component->treeRootId!=instanceRoot)
        {error="Select the root of an imported linked UI hierarchy.";return false;}
        const auto reference=*rootWidget->component;
        ScreenDocument source;std::string root;
        if(!LoadLinkedTreeSource(*this,reference,source,root,error))return false;
        const auto closure=LinkedSourceClosure(source,reference.widgetId);
        std::unordered_map<StableId,StableId> identities;
        for(const auto& widget:Document().widgets)
        {
            if(!widget.component||widget.component->treeRootId!=instanceRoot)continue;
            const auto& link=*widget.component;
            if(link.documentId!=reference.documentId||link.pathHint!=reference.pathHint||
                !closure.count(link.widgetId))
            {error="Linked subtree source member removed or its identity changed; detach or reimport.";return false;}
            const auto src=std::find_if(source.widgets.begin(),source.widgets.end(),
                [&](const auto& s){return s.id==link.widgetId;});
            if(src==source.widgets.end()||src->kind!=widget.kind||
                !identities.emplace(link.widgetId,widget.id).second)
            {error="Linked subtree has a changed kind or duplicate member.";return false;}
        }
        if(identities.find(reference.widgetId)==identities.end()||
            identities.at(reference.widgetId)!=instanceRoot)
        {error="Linked subtree instance root no longer matches its source.";return false;}
        // Existing local parent choices are never overwritten by a source
        // restructure. Treat those conflicts as explicit authoring decisions.
        for(const auto& src:source.widgets)
        {
            const auto it=identities.find(src.id);
            if(it==identities.end()||src.id==reference.widgetId)continue;
            const auto parent=identities.find(src.parentId);
            const auto* local=FindWidget(it->second);
            if(parent==identities.end()||!local||local->parentId!=parent->second)
            {error="Linked subtree hierarchy changed; detach or reimport explicitly.";return false;}
        }
        const auto newCount=closure.size()-identities.size();
        if(Document().widgets.size()+newCount>256)
        {error="Linked subtree refresh exceeds the Screen element limit.";return false;}
        for(const auto& src:source.widgets)
            if(closure.count(src.id)&&!identities.count(src.id))
                identities.emplace(src.id,GenerateStableId());
        auto candidate=Document();
        if(!AppendLinkedTreeMembers(candidate,source,reference,closure,
            identities,instanceRoot,error))return false;
        ScreenDocument effective;
        if(!ResolveScreenTheme(root,candidate,effective,error))return false;
        if(newCount==0){error.clear();return true;}
        return CommitMutation(std::move(candidate),error);
    }
}

namespace renegade::bridge
{
    bool ScreenAuthoringSession::CaptureGeometry(const std::vector<StableId>& ids,
        GeometryCapture& capture, ScreenRect& bounds, std::string& error) const
    {
        if(!loaded_){error="No Screen is open.";return false;}
        std::unordered_set<StableId> selected,roots;
        if(!SelectedRoots(Document(),ids,selected,roots,error))return false;
        GeometryCapture result; bool first=true;
        for(const auto& w:Document().widgets) if(roots.count(w.id))
        {
            const auto* parent=FindWidget(w.parentId);
            if(parent&&EffectiveScreenChildLayout(parent->container,Document().designWidth)!=ScreenChildLayout::Free)
            {error="Selected child geometry is layout-managed.";return false;}
            ScreenRect r;if(!ResolveScreenWidgetRect(Document(),w.id,r,error))return false;
            if(first){bounds=r;first=false;}else{
                const float right=std::max(bounds.x+bounds.width,r.x+r.width),bottom=std::max(bounds.y+bounds.height,r.y+r.height);
                bounds.x=std::min(bounds.x,r.x);bounds.y=std::min(bounds.y,r.y);
                bounds.width=right-bounds.x;bounds.height=bottom-bounds.y;}
        }
        // Free descendants scale with selected groups; managed cells retain their layout authority.
        auto affected=roots;
        bool more=true;while(more){more=false;for(const auto& w:Document().widgets)
            if(affected.count(w.parentId)){
                const auto* parent=FindWidget(w.parentId);
                if(parent&&EffectiveScreenChildLayout(parent->container,Document().designWidth)==ScreenChildLayout::Free)
                    more=affected.insert(w.id).second||more;}}
        for(const auto& w:Document().widgets)if(affected.count(w.id)){
            if(w.component){error="Detach linked components before batch geometry editing.";return false;}
            ScreenRect r;if(!ResolveScreenWidgetRect(Document(),w.id,r,error))return false;
            result.push_back({w.id,r});}
        capture=std::move(result);error.clear();return true;
    }
    bool ScreenAuthoringSession::TransformGeometry(const GeometryCapture& capture,
        const ScreenRect& bounds,const ScreenRect& target,std::string& error)
    {
        if(!loaded_||capture.empty()||!std::isfinite(target.x)||!std::isfinite(target.y)||
            !std::isfinite(target.width)||!std::isfinite(target.height)||bounds.width<=0||bounds.height<=0||target.width<=0||target.height<=0)
        {error="Invalid batch geometry.";return false;}
        auto c=Document();const float sx=target.width/bounds.width,sy=target.height/bounds.height;
        // Apply parents before children, independent of paint order.
        auto pending=capture;std::unordered_set<StableId> done;
        while(!pending.empty()){
            bool progress=false;
            for(auto i=pending.begin();i!=pending.end();){
                auto w=std::find_if(c.widgets.begin(),c.widgets.end(),[&](const auto& w){return w.id==i->first;});
                if(w==c.widgets.end()||w->component){error="Captured selection changed.";return false;}
                const auto parent=std::find_if(c.widgets.begin(),c.widgets.end(),[&](const auto& p){return p.id==w->parentId;});
                if(parent!=c.widgets.end()&&EffectiveScreenChildLayout(parent->container,c.designWidth)!=ScreenChildLayout::Free)
                {error="Captured child is now layout-managed.";return false;}
                if(std::any_of(pending.begin(),pending.end(),[&](const auto& p){return p.first==w->parentId;})){++i;continue;}
                const auto& r=i->second;ScreenRect n{target.x+(r.x-bounds.x)*sx,target.y+(r.y-bounds.y)*sy,r.width*sx,r.height*sy};
                // Preserve intentionally thin dividers/spacers during translation; never
                // shrink an ordinary control below 8px or an authored thin axis below its captured size.
                if(n.width<std::min(8.0f,r.width)||n.height<std::min(8.0f,r.height))
                {error="Batch transform is below an element minimum bound.";return false;}
                if(!SetResolvedGeometry(c,*w,n,error))return false;
                ScreenRect effective;
                if(!ResolveScreenWidgetRect(c,w->id,effective,error))return false;
                if(std::abs(effective.width-n.width)>.01f||std::abs(effective.height-n.height)>.01f)
                {error="Batch transform conflicts with element size constraints.";return false;}
                i=pending.erase(i);progress=true;
            }
            if(!progress){error="Invalid captured parent graph.";return false;}
        }
        return CommitMutation(std::move(c),error);
    }
}
