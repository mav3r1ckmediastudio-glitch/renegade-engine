#pragma once

#include "renegade/bridge/FlowService.h"

#include <cstddef>
#include <string>
#include <vector>

namespace renegade::bridge
{
    // Gate 3 semantic authoring boundary. It owns one validated Flow document,
    // a Flow-specific history, dirty/save state and transactional persistence.
    // Presentation layout remains outside this class by design.
    class StoryFlowAuthoringSession final
    {
    public:
        [[nodiscard]] bool Open(
            const std::string& filePath,
            const StableId& expectedProjectId,
            std::string& error);
        [[nodiscard]] bool Reload(std::string& error);
        [[nodiscard]] bool Save(std::string& error);
        void Clear() noexcept;

        [[nodiscard]] bool RenameNode(
            const StableId& nodeId,
            std::string name,
            std::string& error);
        [[nodiscard]] bool AddNode(
            FlowNode node,
            StableId& createdNodeId,
            std::string& error);
        [[nodiscard]] bool DeleteNode(
            const StableId& nodeId,
            std::string& error);

        [[nodiscard]] bool AddRoute(
            FlowRoute route,
            StableId& createdRouteId,
            std::string& error);
        [[nodiscard]] bool UpdateRoute(
            const StableId& routeId,
            FlowRoute route,
            std::string& error);
        [[nodiscard]] bool DeleteRoute(
            const StableId& routeId,
            std::string& error);

        [[nodiscard]] bool Undo(std::string& error);
        [[nodiscard]] bool Redo(std::string& error);

        [[nodiscard]] bool IsLoaded() const noexcept;
        [[nodiscard]] bool IsDirty() const noexcept;
        [[nodiscard]] bool CanUndo() const noexcept;
        [[nodiscard]] bool CanRedo() const noexcept;
        [[nodiscard]] std::size_t UndoCount() const noexcept;
        [[nodiscard]] std::size_t RedoCount() const noexcept;
        [[nodiscard]] const FlowDocument& Document() const noexcept;
        [[nodiscard]] const std::string& FilePath() const noexcept;
        [[nodiscard]] const StableId& ProjectId() const noexcept;

    private:
        static constexpr std::size_t InvalidHistoryIndex =
            static_cast<std::size_t>(-1);
        static constexpr std::size_t MaximumHistoryEntries = 256;

        [[nodiscard]] bool Adopt(
            FlowDocument document,
            const StableId& expectedProjectId,
            std::string filePath,
            std::string& error);
        [[nodiscard]] bool CommitMutation(
            FlowDocument document,
            std::string& error);

        FlowDocument emptyDocument_;
        std::vector<FlowDocument> history_;
        std::size_t historyIndex_ = 0;
        std::size_t savedHistoryIndex_ = InvalidHistoryIndex;
        StableId projectId_;
        std::string filePath_;
        bool loaded_ = false;
    };
}
