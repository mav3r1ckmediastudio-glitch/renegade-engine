#pragma once

#include "renegade/bridge/ScreenService.h"

#include <algorithm>
#include <cstddef>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace renegade::bridge
{
    struct ScreenWidgetAuthoringEdit
    {
        std::string name;
        std::string text;
        ScreenRect resolvedRect;
        bool visible = true;
        bool enabled = true;
    };

    struct ScreenWidgetCreatorEdit
    {
        std::string resourcePath;
        std::string actionId;
        StableId parentId;
        ScreenLayoutMode layoutMode = ScreenLayoutMode::Absolute;
        ScreenAnchors anchors;
        ScreenWidgetStyle style;
    };

    // UI-independent Screen document mutation boundary. Gate 8C established
    // validated property editing/history/persistence; Gate 8D extends the same
    // authority to creator-level element transactions without giving Studio a
    // second mutation path around the governed Screen document.
    class ScreenAuthoringSession final
    {
    public:
        [[nodiscard]] bool Open(
            const std::string& filePath,
            const StableId& expectedProjectId,
            std::string& error);
        [[nodiscard]] bool Reload(std::string& error);
        [[nodiscard]] bool Save(std::string& error);
        void Clear() noexcept;

        [[nodiscard]] bool UpdateWidget(
            const StableId& widgetId,
            ScreenWidgetAuthoringEdit edit,
            std::string& error);

        // Direct preview manipulation updates geometry every frame.
        // Coalescing keeps one complete pointer gesture as one Undo.
        void BeginCoalescedEdit() noexcept
        {
            coalescedEditActive_ = true;
            coalescedMutationCreated_ = false;
        }
        void EndCoalescedEdit() noexcept
        {
            coalescedEditActive_ = false;
            coalescedMutationCreated_ = false;
        }

        // Complete Gate 8D presentation/binding transaction. The existing 8C
        // basic edit remains untouched; this additive path owns every remaining
        // persistent schema-v2 property and commits through the same history.
        [[nodiscard]] bool UpdateWidgetCreatorFields(
            const StableId& widgetId,
            ScreenWidgetCreatorEdit edit,
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
            if (edit.parentId == widgetId)
            {
                error = "A Screen widget cannot parent itself.";
                return false;
            }

            if (!edit.parentId.empty())
            {
                StableId ancestor = edit.parentId;
                std::unordered_set<StableId> visited;
                while (!ancestor.empty())
                {
                    if (ancestor == widgetId)
                    {
                        error = "A Screen widget cannot be parented to one of its descendants.";
                        return false;
                    }
                    if (!visited.insert(ancestor).second)
                    {
                        error = "The proposed Screen parent graph already contains a cycle.";
                        return false;
                    }
                    const auto parent = std::find_if(
                        candidate.widgets.begin(), candidate.widgets.end(),
                        [&ancestor](const ScreenWidget& widget)
                        {
                            return widget.id == ancestor;
                        });
                    if (parent == candidate.widgets.end())
                    {
                        error = "The proposed Screen parent does not exist: " + ancestor;
                        return false;
                    }
                    ancestor = parent->parentId;
                }
            }

            ScreenRect resolvedBefore;
            if (!ResolveScreenWidgetRect(
                    candidate, widgetId, resolvedBefore, error))
            {
                return false;
            }

            found->resourcePath = std::move(edit.resourcePath);
            found->actionId = std::move(edit.actionId);
            found->parentId = std::move(edit.parentId);
            found->layoutMode = edit.layoutMode;
            found->anchors = edit.anchors;
            found->style = std::move(edit.style);

            ScreenRect parentRect{
                0.0f, 0.0f, candidate.designWidth, candidate.designHeight};
            if (!found->parentId.empty() &&
                !ResolveScreenWidgetRect(
                    candidate, found->parentId, parentRect, error))
            {
                return false;
            }
            const float relativeX = resolvedBefore.x - parentRect.x;
            const float relativeY = resolvedBefore.y - parentRect.y;
            if (found->layoutMode == ScreenLayoutMode::Absolute)
            {
                found->rect = {
                    relativeX,
                    relativeY,
                    resolvedBefore.width,
                    resolvedBefore.height,
                };
            }
            else
            {
                // Resolved preview geometry remains authoritative for anchor
                // offsets. Editing anchor min/max or changing parent therefore
                // does not make the element jump on the design canvas.
                found->anchors.offsetMinimumX = relativeX -
                    found->anchors.minimumX * parentRect.width;
                found->anchors.offsetMinimumY = relativeY -
                    found->anchors.minimumY * parentRect.height;
                found->anchors.offsetMaximumX =
                    relativeX + resolvedBefore.width -
                    found->anchors.maximumX * parentRect.width;
                found->anchors.offsetMaximumY =
                    relativeY + resolvedBefore.height -
                    found->anchors.maximumY * parentRect.height;
            }

            return CommitMutation(std::move(candidate), error);
        }

        // Symbolic Screen action identity is authored here; Story Flow remains
        // the sole authority that later routes those IDs to destinations.
        [[nodiscard]] bool AddAction(
            std::string actionId,
            std::string& error)
        {
            if (!loaded_)
            {
                error = "Screen authoring session is not open.";
                return false;
            }
            if (actionId.empty())
            {
                error = "A Screen action ID cannot be empty.";
                return false;
            }
            ScreenDocument candidate = Document();
            if (candidate.actions.size() >= 64)
            {
                error = "The Screen action limit has been reached.";
                return false;
            }
            if (std::any_of(candidate.actions.begin(), candidate.actions.end(),
                    [&actionId](const ScreenAction& action)
                    {
                        return action.id == actionId;
                    }))
            {
                error = "Screen action already exists: " + actionId;
                return false;
            }
            candidate.actions.push_back({std::move(actionId)});
            return CommitMutation(std::move(candidate), error);
        }

        [[nodiscard]] bool RenameAction(
            const std::string& actionId,
            std::string replacementId,
            std::string& error)
        {
            if (!loaded_)
            {
                error = "Screen authoring session is not open.";
                return false;
            }
            if (replacementId.empty())
            {
                error = "A Screen action ID cannot be empty.";
                return false;
            }
            ScreenDocument candidate = Document();
            const auto found = std::find_if(
                candidate.actions.begin(), candidate.actions.end(),
                [&actionId](const ScreenAction& action)
                {
                    return action.id == actionId;
                });
            if (found == candidate.actions.end())
            {
                error = "Screen action does not exist: " + actionId;
                return false;
            }
            if (replacementId != actionId &&
                std::any_of(candidate.actions.begin(), candidate.actions.end(),
                    [&replacementId](const ScreenAction& action)
                    {
                        return action.id == replacementId;
                    }))
            {
                error = "Screen action already exists: " + replacementId;
                return false;
            }

            found->id = replacementId;
            for (auto& widget : candidate.widgets)
            {
                if (widget.kind == ScreenWidgetKind::Button &&
                    widget.actionId == actionId)
                {
                    widget.actionId = replacementId;
                }
            }
            return CommitMutation(std::move(candidate), error);
        }

        [[nodiscard]] bool DeleteAction(
            const std::string& actionId,
            std::string& error)
        {
            if (!loaded_)
            {
                error = "Screen authoring session is not open.";
                return false;
            }
            ScreenDocument candidate = Document();
            const auto found = std::find_if(
                candidate.actions.begin(), candidate.actions.end(),
                [&actionId](const ScreenAction& action)
                {
                    return action.id == actionId;
                });
            if (found == candidate.actions.end())
            {
                error = "Screen action does not exist: " + actionId;
                return false;
            }
            if (candidate.actions.size() <= 1)
            {
                error = "A Screen must retain at least one action.";
                return false;
            }
            if (std::any_of(candidate.widgets.begin(), candidate.widgets.end(),
                    [&actionId](const ScreenWidget& widget)
                    {
                        return widget.kind == ScreenWidgetKind::Button &&
                            widget.actionId == actionId;
                    }))
            {
                error = "Screen action is still referenced by a Button: " + actionId;
                return false;
            }
            candidate.actions.erase(found);
            return CommitMutation(std::move(candidate), error);
        }

        [[nodiscard]] bool MoveButtonFocusEarlier(
            const StableId& widgetId,
            std::string& error)
        {
            return MoveButtonFocus(widgetId, -1, error);
        }

        [[nodiscard]] bool MoveButtonFocusLater(
            const StableId& widgetId,
            std::string& error)
        {
            return MoveButtonFocus(widgetId, 1, error);
        }

        // Gate 8D creator transactions. Every operation mutates a complete
        // candidate document, validates it, then enters the same bounded
        // Screen Undo/Redo history as Inspector edits.
        [[nodiscard]] bool CreateWidget(
            ScreenWidget widget,
            bool insertAtBack,
            StableId& createdWidgetId,
            std::string& error);
        [[nodiscard]] bool DuplicateWidget(
            const StableId& widgetId,
            StableId& duplicatedWidgetId,
            std::string& error);

        // Reusable components are authored Screen subtrees, not a competing
        // file format. This transaction duplicates one root plus descendants,
        // remaps every parent link and gives every copied element a fresh ID.
        [[nodiscard]] bool DuplicateWidgetTree(
            const StableId& widgetId,
            StableId& duplicatedRootId,
            std::string& error)
        {
            duplicatedRootId.clear();
            if (!loaded_)
            {
                error = "Screen authoring session is not open.";
                return false;
            }

            ScreenDocument candidate = Document();
            const auto root = std::find_if(
                candidate.widgets.begin(), candidate.widgets.end(),
                [&widgetId](const ScreenWidget& widget)
                {
                    return widget.id == widgetId;
                });
            if (root == candidate.widgets.end())
            {
                error = "Screen widget does not exist: " + widgetId;
                return false;
            }

            std::unordered_set<StableId> subtree{widgetId};
            bool expanded = true;
            while (expanded)
            {
                expanded = false;
                for (const auto& widget : candidate.widgets)
                {
                    if (!widget.parentId.empty() &&
                        subtree.count(widget.parentId) != 0 &&
                        subtree.insert(widget.id).second)
                    {
                        expanded = true;
                    }
                }
            }
            if (candidate.widgets.size() + subtree.size() > 256)
            {
                error = "Reusable component duplication would exceed the Screen element limit.";
                return false;
            }

            std::unordered_map<StableId, StableId> remap;
            remap.reserve(subtree.size());
            for (const auto& widget : candidate.widgets)
            {
                if (subtree.count(widget.id) != 0)
                    remap.emplace(widget.id, GenerateStableId());
            }

            std::vector<ScreenWidget> duplicates;
            duplicates.reserve(subtree.size());
            for (const auto& source : candidate.widgets)
            {
                const auto mapped = remap.find(source.id);
                if (mapped == remap.end()) continue;
                ScreenWidget duplicate = source;
                duplicate.id = mapped->second;
                if (source.id == widgetId)
                    duplicate.name += " Copy";
                const auto mappedParent = remap.find(source.parentId);
                if (mappedParent != remap.end())
                    duplicate.parentId = mappedParent->second;
                duplicates.push_back(std::move(duplicate));
            }
            candidate.widgets.insert(
                candidate.widgets.end(), duplicates.begin(), duplicates.end());

            std::vector<StableId> newFocusOrder;
            newFocusOrder.reserve(candidate.focusOrder.size() + subtree.size());
            for (const auto& focusId : candidate.focusOrder)
            {
                newFocusOrder.push_back(focusId);
                const auto mapped = remap.find(focusId);
                if (mapped != remap.end())
                    newFocusOrder.push_back(mapped->second);
            }
            candidate.focusOrder = std::move(newFocusOrder);

            const auto copiedRoot = remap.find(widgetId);
            if (copiedRoot == remap.end())
            {
                error = "Reusable component root could not be remapped.";
                return false;
            }
            const StableId newRootId = copiedRoot->second;
            if (!CommitMutation(std::move(candidate), error)) return false;
            duplicatedRootId = newRootId;
            return true;
        }

        [[nodiscard]] bool DeleteWidget(
            const StableId& widgetId,
            std::string& error);
        [[nodiscard]] bool MoveWidgetToBack(
            const StableId& widgetId,
            std::string& error);
        [[nodiscard]] bool MoveWidgetToFront(
            const StableId& widgetId,
            std::string& error);

        [[nodiscard]] bool Undo(std::string& error);
        [[nodiscard]] bool Redo(std::string& error);

        [[nodiscard]] bool IsLoaded() const noexcept;
        [[nodiscard]] bool IsDirty() const noexcept;
        [[nodiscard]] bool CanUndo() const noexcept;
        [[nodiscard]] bool CanRedo() const noexcept;
        [[nodiscard]] std::size_t UndoCount() const noexcept;
        [[nodiscard]] std::size_t RedoCount() const noexcept;
        [[nodiscard]] const ScreenDocument& Document() const noexcept;
        [[nodiscard]] const ScreenWidget* FindWidget(
            const StableId& widgetId) const noexcept;
        [[nodiscard]] const std::string& FilePath() const noexcept;
        [[nodiscard]] const StableId& ProjectId() const noexcept;

    private:
        static constexpr std::size_t InvalidHistoryIndex =
            static_cast<std::size_t>(-1);
        static constexpr std::size_t MaximumHistoryEntries = 256;

        [[nodiscard]] bool MoveButtonFocus(
            const StableId& widgetId,
            const int direction,
            std::string& error)
        {
            if (!loaded_)
            {
                error = "Screen authoring session is not open.";
                return false;
            }
            ScreenDocument candidate = Document();
            const auto widget = std::find_if(
                candidate.widgets.begin(), candidate.widgets.end(),
                [&widgetId](const ScreenWidget& value)
                {
                    return value.id == widgetId;
                });
            if (widget == candidate.widgets.end() ||
                widget->kind != ScreenWidgetKind::Button)
            {
                error = "Screen focus order can only move Button widgets.";
                return false;
            }
            const auto focus = std::find(
                candidate.focusOrder.begin(), candidate.focusOrder.end(), widgetId);
            if (focus == candidate.focusOrder.end())
            {
                error = "The Button is missing from Screen focus order.";
                return false;
            }
            const std::ptrdiff_t index = std::distance(
                candidate.focusOrder.begin(), focus);
            const std::ptrdiff_t target = index + direction;
            if (target < 0 ||
                target >= static_cast<std::ptrdiff_t>(candidate.focusOrder.size()))
            {
                error = direction < 0
                    ? "The Button is already first in focus order."
                    : "The Button is already last in focus order.";
                return false;
            }
            std::swap(candidate.focusOrder[static_cast<std::size_t>(index)],
                candidate.focusOrder[static_cast<std::size_t>(target)]);
            return CommitMutation(std::move(candidate), error);
        }

        [[nodiscard]] bool Adopt(
            ScreenDocument document,
            const StableId& expectedProjectId,
            std::string filePath,
            std::string& error);
        [[nodiscard]] bool CommitMutation(
            ScreenDocument document,
            std::string& error);

        ScreenDocument emptyDocument_;
        std::vector<ScreenDocument> history_;
        std::size_t historyIndex_ = 0;
        std::size_t savedHistoryIndex_ = InvalidHistoryIndex;
        StableId projectId_;
        std::string filePath_;
        bool loaded_ = false;
        bool coalescedEditActive_ = false;
        bool coalescedMutationCreated_ = false;
    };
}
