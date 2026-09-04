#pragma once

#include <string>
#include <utility>

#include "renegade/bridge/CommandService.h"
#include "renegade/bridge/ScriptAuthoringService.h"
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

        [[nodiscard]] const SelectionService& Selection() const noexcept
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

        [[nodiscard]] ScriptAuthoringService& Scripts() noexcept
        {
            return scripts_;
        }

        [[nodiscard]] const ScriptAuthoringService& Scripts() const noexcept
        {
            return scripts_;
        }

        void NewScene()
        {
            documents_.NewScene();
            scripts_.Invalidate();
        }

        bool CommitPendingProjectScene(PreparedSceneOpen prepared)
        {
            if (!projects_.HasPendingProject())
            {
                const bool committed =
                    documents_.CommitPreparedOpen(std::move(prepared));
                if (committed)
                    scripts_.Invalidate();
                return committed;
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

            const bool committed =
                documents_.CommitPreparedOpen(std::move(prepared));
            if (committed)
                scripts_.Invalidate();
            return committed;
        }

        // Gate 7 Story Flow-native adoption boundary. The pending descriptor
        // has already had its startup Flow resolved and parsed. Commit project
        // authority first, then clear any Scene belonging to the previous
        // project so it cannot leak into the newly active context.
        bool CommitPendingProjectWithoutScene()
        {
            if (!projects_.HasPendingProject())
            {
                scenes_.SetLastError(
                    "No Story Flow project is pending adoption.");
                return false;
            }
            if (!projects_.PendingProject().startupScene.empty())
            {
                scenes_.SetLastError(
                    "Scene-first projects require the prepared Scene adoption boundary.");
                projects_.DiscardPendingProject();
                return false;
            }
            if (!projects_.CommitPendingProject())
            {
                scenes_.SetLastError(
                    "Project adoption failed after startup-Flow validation: " +
                    projects_.LastError());
                projects_.DiscardPendingProject();
                return false;
            }
            documents_.NewScene();
            scripts_.Invalidate();
            return true;
        }

        bool LoadScene(const std::string& filePath)
        {
            return CommitPendingProjectScene(documents_.PrepareOpen(filePath));
        }

        bool SaveScene(const std::string& filePath)
        {
            const std::string previousPath = scenes_.CurrentPath();
            if (!documents_.Save(filePath))
                return false;

            std::string scriptError;
            if (!scripts_.SaveForScene(filePath, previousPath, scriptError))
            {
                // SceneDocumentService has already marked the shared command
                // history saved. A failed companion commit means the complete
                // creator document transaction is not saved, so restore dirty
                // state and surface one authoritative error.
                commands_.MarkUnsaved();
                scenes_.SetLastError(
                    "Scene saved but scripting companion failed: " +
                    scriptError);
                return false;
            }
            return true;
        }

        bool ReloadScene()
        {
            const bool reloaded = documents_.Reload();
            if (reloaded)
                scripts_.Invalidate();
            return reloaded;
        }

    private:
        inline static StudioSession* current_ = nullptr;

        StudioProjectService projects_;
        SceneService scenes_;
        SelectionService selection_;
        CommandService commands_;
        SceneDocumentService documents_{
            scenes_, selection_, commands_, projects_};
        ScriptAuthoringService scripts_{scenes_, projects_, commands_};
    };
}
