#pragma once

#include "renegade/bridge/ScreenService.h"

#include <cstddef>
#include <string>
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
