#include "renegade/bridge/StoryFlowAuthoringSession.h"

#include <algorithm>
#include <utility>

namespace renegade::bridge
{
    bool StoryFlowAuthoringSession::Open(
        const std::string& filePath,
        const StableId& expectedProjectId,
        std::string& error)
    {
        FlowDocument document;
        if (!ReadFlowDocument(filePath, expectedProjectId, document, error))
        {
            return false;
        }
        return Adopt(
            std::move(document),
            expectedProjectId,
            filePath,
            error);
    }

    bool StoryFlowAuthoringSession::Reload(std::string& error)
    {
        if (!loaded_ || filePath_.empty())
        {
            error = "Story Flow authoring session is not open.";
            return false;
        }

        FlowDocument document;
        if (!ReadFlowDocument(filePath_, projectId_, document, error))
        {
            return false;
        }
        return Adopt(
            std::move(document),
            projectId_,
            filePath_,
            error);
    }

    bool StoryFlowAuthoringSession::Save(std::string& error)
    {
        if (!loaded_ || filePath_.empty())
        {
            error = "Story Flow authoring session is not open.";
            return false;
        }
        if (!WriteFlowDocument(filePath_, Document(), error))
        {
            return false;
        }
        savedHistoryIndex_ = historyIndex_;
        error.clear();
        return true;
    }

    void StoryFlowAuthoringSession::Clear() noexcept
    {
        history_.clear();
        historyIndex_ = 0;
        savedHistoryIndex_ = InvalidHistoryIndex;
        projectId_.clear();
        filePath_.clear();
        loaded_ = false;
    }

    bool StoryFlowAuthoringSession::RenameNode(
        const StableId& nodeId,
        std::string name,
        std::string& error)
    {
        if (!loaded_)
        {
            error = "Story Flow authoring session is not open.";
            return false;
        }

        FlowDocument candidate = Document();
        const auto found = std::find_if(
            candidate.nodes.begin(),
            candidate.nodes.end(),
            [&nodeId](const FlowNode& node)
            {
                return node.id == nodeId;
            });
        if (found == candidate.nodes.end())
        {
            error = "Story Flow node does not exist: " + nodeId;
            return false;
        }
        if (found->name == name)
        {
            error.clear();
            return true;
        }
        found->name = std::move(name);
        return CommitMutation(std::move(candidate), error);
    }

    bool StoryFlowAuthoringSession::AddNode(
        FlowNode node,
        StableId& createdNodeId,
        std::string& error)
    {
        createdNodeId.clear();
        if (!loaded_)
        {
            error = "Story Flow authoring session is not open.";
            return false;
        }
        if (node.kind == FlowNodeKind::GameStart)
        {
            error = "The permanent Game Start node cannot be created by an authoring command.";
            return false;
        }
        if (node.id.empty())
        {
            node.id = GenerateStableId();
        }

        FlowDocument candidate = Document();
        const StableId proposedId = node.id;
        candidate.nodes.push_back(std::move(node));
        if (!CommitMutation(std::move(candidate), error))
        {
            return false;
        }
        createdNodeId = proposedId;
        return true;
    }

    bool StoryFlowAuthoringSession::DeleteNode(
        const StableId& nodeId,
        std::string& error)
    {
        if (!loaded_)
        {
            error = "Story Flow authoring session is not open.";
            return false;
        }

        FlowDocument candidate = Document();
        const auto found = std::find_if(
            candidate.nodes.begin(),
            candidate.nodes.end(),
            [&nodeId](const FlowNode& node)
            {
                return node.id == nodeId;
            });
        if (found == candidate.nodes.end())
        {
            error = "Story Flow node does not exist: " + nodeId;
            return false;
        }
        if (found->kind == FlowNodeKind::GameStart ||
            candidate.startNodeId == nodeId)
        {
            error = "The permanent Game Start node cannot be deleted.";
            return false;
        }

        candidate.nodes.erase(found);
        candidate.routes.erase(
            std::remove_if(
                candidate.routes.begin(),
                candidate.routes.end(),
                [&nodeId](const FlowRoute& route)
                {
                    return route.sourceNodeId == nodeId ||
                        route.destinationNodeId == nodeId;
                }),
            candidate.routes.end());
        return CommitMutation(std::move(candidate), error);
    }

    bool StoryFlowAuthoringSession::AddRoute(
        FlowRoute route,
        StableId& createdRouteId,
        std::string& error)
    {
        createdRouteId.clear();
        if (!loaded_)
        {
            error = "Story Flow authoring session is not open.";
            return false;
        }
        if (route.id.empty())
        {
            route.id = GenerateStableId();
        }

        FlowDocument candidate = Document();
        const StableId proposedId = route.id;
        candidate.routes.push_back(std::move(route));
        if (!CommitMutation(std::move(candidate), error))
        {
            return false;
        }
        createdRouteId = proposedId;
        return true;
    }

    bool StoryFlowAuthoringSession::UpdateRoute(
        const StableId& routeId,
        FlowRoute route,
        std::string& error)
    {
        if (!loaded_)
        {
            error = "Story Flow authoring session is not open.";
            return false;
        }
        if (!route.id.empty() && route.id != routeId)
        {
            error = "A Story Flow route edit cannot change stable route identity.";
            return false;
        }

        FlowDocument candidate = Document();
        const auto found = std::find_if(
            candidate.routes.begin(),
            candidate.routes.end(),
            [&routeId](const FlowRoute& item)
            {
                return item.id == routeId;
            });
        if (found == candidate.routes.end())
        {
            error = "Story Flow route does not exist: " + routeId;
            return false;
        }
        route.id = routeId;
        *found = std::move(route);
        return CommitMutation(std::move(candidate), error);
    }

    bool StoryFlowAuthoringSession::DeleteRoute(
        const StableId& routeId,
        std::string& error)
    {
        if (!loaded_)
        {
            error = "Story Flow authoring session is not open.";
            return false;
        }

        FlowDocument candidate = Document();
        const auto found = std::find_if(
            candidate.routes.begin(),
            candidate.routes.end(),
            [&routeId](const FlowRoute& route)
            {
                return route.id == routeId;
            });
        if (found == candidate.routes.end())
        {
            error = "Story Flow route does not exist: " + routeId;
            return false;
        }
        candidate.routes.erase(found);
        return CommitMutation(std::move(candidate), error);
    }

    bool StoryFlowAuthoringSession::Undo(std::string& error)
    {
        if (!CanUndo())
        {
            error = "Story Flow has no authoring command to undo.";
            return false;
        }
        --historyIndex_;
        error.clear();
        return true;
    }

    bool StoryFlowAuthoringSession::Redo(std::string& error)
    {
        if (!CanRedo())
        {
            error = "Story Flow has no authoring command to redo.";
            return false;
        }
        ++historyIndex_;
        error.clear();
        return true;
    }

    bool StoryFlowAuthoringSession::IsLoaded() const noexcept
    {
        return loaded_;
    }

    bool StoryFlowAuthoringSession::IsDirty() const noexcept
    {
        return loaded_ &&
            (savedHistoryIndex_ == InvalidHistoryIndex ||
             savedHistoryIndex_ != historyIndex_);
    }

    bool StoryFlowAuthoringSession::CanUndo() const noexcept
    {
        return loaded_ && historyIndex_ > 0;
    }

    bool StoryFlowAuthoringSession::CanRedo() const noexcept
    {
        return loaded_ && historyIndex_ + 1 < history_.size();
    }

    std::size_t StoryFlowAuthoringSession::UndoCount() const noexcept
    {
        return CanUndo() ? historyIndex_ : 0;
    }

    std::size_t StoryFlowAuthoringSession::RedoCount() const noexcept
    {
        return CanRedo() ? history_.size() - historyIndex_ - 1 : 0;
    }

    const FlowDocument& StoryFlowAuthoringSession::Document() const noexcept
    {
        return loaded_ && historyIndex_ < history_.size()
            ? history_[historyIndex_]
            : emptyDocument_;
    }

    const std::string& StoryFlowAuthoringSession::FilePath() const noexcept
    {
        return filePath_;
    }

    const StableId& StoryFlowAuthoringSession::ProjectId() const noexcept
    {
        return projectId_;
    }

    bool StoryFlowAuthoringSession::Adopt(
        FlowDocument document,
        const StableId& expectedProjectId,
        std::string filePath,
        std::string& error)
    {
        if (!ValidateFlowDocument(document, expectedProjectId, error))
        {
            return false;
        }
        if (filePath.empty())
        {
            error = "Story Flow authoring requires a document path.";
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

    bool StoryFlowAuthoringSession::CommitMutation(
        FlowDocument document,
        std::string& error)
    {
        if (!loaded_)
        {
            error = "Story Flow authoring session is not open.";
            return false;
        }
        if (!ValidateFlowDocument(document, projectId_, error))
        {
            return false;
        }

        if (historyIndex_ + 1 < history_.size())
        {
            if (savedHistoryIndex_ != InvalidHistoryIndex &&
                savedHistoryIndex_ > historyIndex_)
            {
                savedHistoryIndex_ = InvalidHistoryIndex;
            }
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
                {
                    savedHistoryIndex_ = InvalidHistoryIndex;
                }
                else
                {
                    --savedHistoryIndex_;
                }
            }
        }

        error.clear();
        return true;
    }
}
