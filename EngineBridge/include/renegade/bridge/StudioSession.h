#pragma once

#include <utility>

#include "renegade/bridge/CommandService.h"
#include "renegade/bridge/StudioProjectService.h"
#include "renegade/bridge/SceneService.h"
#include "renegade/bridge/SceneDocumentService.h"
#include "renegade/bridge/SelectionService.h"

namespace renegade::bridge
{
    class StudioSession
    {
    public:
        StudioSession() noexcept
        {
            current_ = this;
        }

        ~StudioSession()
        {
            if (current_ == this)
            {
                current_ = nullptr;
            }
        }

        StudioSession(const StudioSession&) = delete;
        StudioSession& operator=(const StudioSession&) = delete;
        StudioSession(StudioSession&&) = delete;
        StudioSession& operator=(StudioSession&&) = delete;

        [[nodiscard]] static StudioSession* Current() noexcept
        {
            return current_;
        }

        [[nodiscard]] SceneService& Scenes() noexcept
        {
            return scenes_;
        }

        [[nodiscard]] const SceneService& Scenes() const noexcept
        {
            return scenes_;
        }

        [[nodiscard]] SelectionService& Selection() noexcept
        {
            return selection_;
        }

        [[nodiscard]] SceneDocumentService& Documents() noexcept
        {
            return documents_;
        }

        [[nodiscard]] const SceneDocumentService& Documents() const noexcept
        {
            return documents_;
        }

        [[nodiscard]] CommandService& Commands() noexcept
        {
            return commands_;
        }

        [[nodiscard]] StudioProjectService& Projects() noexcept
        {
            return projects_;
        }

        [[nodiscard]] const StudioProjectService& Projects() const noexcept
        {
            return projects_;
        }

        void NewScene()
        {
            documents_.NewScene();
        }

        bool CommitPendingProjectScene(PreparedSceneOpen prepared)
        {
            if (!projects_.HasPendingProject())
            {
                return documents_.CommitPreparedOpen(std::move(prepared));
            }

            if (!prepared.IsReady())
            {
                const bool ignored =
                    documents_.CommitPreparedOpen(std::move(prepared));
                (void)ignored;
                projects_.DiscardPendingProject();
                return false;
            }

            if (!projects_.CommitPendingProject())
            {
                scenes_.SetLastError(
                    "Project adoption failed after startup-scene validation: " +
                    projects_.LastError());
                projects_.DiscardPendingProject();
                return false;
            }

            return documents_.CommitPreparedOpen(std::move(prepared));
        }

        bool LoadScene(const std::string& filePath)
        {
            return CommitPendingProjectScene(documents_.PrepareOpen(filePath));
        }

        bool SaveScene(const std::string& filePath)
        {
            return documents_.Save(filePath);
        }

        bool ReloadScene()
        {
            return documents_.Reload();
        }

    private:
        inline static StudioSession* current_ = nullptr;

        StudioProjectService projects_;
        SceneService scenes_;
        SelectionService selection_;
        CommandService commands_;
        SceneDocumentService documents_{
            scenes_, selection_, commands_, projects_};
    };
}
