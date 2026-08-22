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
                // The visible X/Y/W/H controls remain authoritative for anchor
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
    };
}
