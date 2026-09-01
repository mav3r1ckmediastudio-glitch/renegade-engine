#pragma once

#include <algorithm>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

#include "renegade/bridge/GameplayInputService.h"
#include "renegade/bridge/ProjectService.h"

namespace renegade::bridge
{
    // Studio-only transactional facade over ProjectService.
    //
    // ProjectService remains the authoritative project persistence service.
    // This facade prevents Studio from changing its active project identity or
    // Recent Projects list until the requested launch root has been validated.
    // The candidate project is validated/migrated in an isolated ProjectService
    // instance first; CommitPendingProject() performs the real adoption only at
    // the Scene or Story Flow replacement boundary.
    class StudioProjectService final : public ProjectService
    {
    public:
        bool CreateProject(
            const std::string& parentDirectory,
            const std::string& projectName,
            const std::string& templateScenePath)
        {
            ProjectService candidate;
            if (!candidate.CreateProject(
                    parentDirectory, projectName, templateScenePath))
            {
                FailPending(candidate.LastError(), candidate.LastWarning());
                return false;
            }
            std::string gameplayError;
            if (!PrepareGameplayInput(candidate, gameplayError))
            {
                FailPending(std::move(gameplayError), candidate.LastWarning());
                return false;
            }
            Stage(candidate.CurrentProject(), candidate.LastWarning());
            return true;
        }

        bool CreateStoryFlowProject(
            const std::string& parentDirectory,
            const std::string& projectName)
        {
            ProjectService candidate;
            if (!candidate.CreateStoryFlowProject(
                    parentDirectory, projectName))
            {
                FailPending(candidate.LastError(), candidate.LastWarning());
                return false;
            }
            std::string gameplayError;
            if (!PrepareGameplayInput(candidate, gameplayError))
            {
                FailPending(std::move(gameplayError), candidate.LastWarning());
                return false;
            }
            Stage(candidate.CurrentProject(), candidate.LastWarning());
            return true;
        }

        bool OpenProject(const std::string& descriptorPath)
        {
            ProjectService candidate;
            if (!candidate.OpenProject(descriptorPath))
            {
                FailPending(candidate.LastError(), candidate.LastWarning());
                return false;
            }
            std::string gameplayError;
            if (!PrepareGameplayInput(candidate, gameplayError))
            {
                FailPending(std::move(gameplayError), candidate.LastWarning());
                return false;
            }
            Stage(candidate.CurrentProject(), candidate.LastWarning());
            return true;
        }

        [[nodiscard]] bool HasPendingProject() const noexcept
        {
            return hasPendingProject_;
        }

        [[nodiscard]] const ProjectMetadata& PendingProject() const noexcept
        {
            return pendingProject_;
        }

        [[nodiscard]] std::string StartupScenePath() const
        {
            const ProjectMetadata* project = hasPendingProject_
                ? &pendingProject_
                : (HasProject() ? &CurrentProject() : nullptr);
            if (project == nullptr)
                return {};
            if (project->startupScene.empty())
                return {};
            return (
                std::filesystem::u8path(project->rootPath) /
                std::filesystem::u8path(project->startupScene))
                .lexically_normal()
                .generic_u8string();
        }

        bool CommitPendingProject()
        {
            if (!hasPendingProject_)
                return true;

            const std::string descriptor = pendingProject_.descriptorPath;
            if (!ProjectService::OpenProject(descriptor))
            {
                pendingError_ = ProjectService::LastError();
                pendingWarning_ = ProjectService::LastWarning();
                return false;
            }

            pendingProject_ = {};
            hasPendingProject_ = false;
            pendingError_.clear();
            pendingWarning_.clear();
            return true;
        }

        // Refresh metadata for the already-authoritative project without
        // entering the staged project-switch lifecycle. This is required when
        // a governed service transactionally updates the active descriptor
        // while its currently loaded Scene remains authoritative.
        bool RefreshCurrentProject()
        {
            if (hasPendingProject_)
            {
                pendingError_ =
                    "Could not refresh the active project while another project is pending.";
                return false;
            }
            if (!HasProject())
            {
                pendingError_ =
                    "Could not refresh project metadata without an active project.";
                return false;
            }

            const std::string descriptor = CurrentProject().descriptorPath;
            if (!ProjectService::OpenProject(descriptor))
            {
                pendingError_ = ProjectService::LastError();
                pendingWarning_ = ProjectService::LastWarning();
                return false;
            }

            pendingError_.clear();
            pendingWarning_.clear();
            return true;
        }

        void DiscardPendingProject() noexcept
        {
            pendingProject_ = {};
            hasPendingProject_ = false;
            pendingError_.clear();
            pendingWarning_.clear();
        }

        [[nodiscard]] const std::string& LastError() const noexcept
        {
            return pendingError_.empty()
                ? ProjectService::LastError()
                : pendingError_;
        }

        [[nodiscard]] const std::string& LastWarning() const noexcept
        {
            return pendingWarning_.empty()
                ? ProjectService::LastWarning()
                : pendingWarning_;
        }

    private:
        [[nodiscard]] static bool PrepareGameplayInput(
            ProjectService& candidate,
            std::string& error)
        {
            if (!candidate.HasProject())
            {
                error = "Could not prepare gameplay input without an active candidate project.";
                return false;
            }

            GameplayInputMap input;
            bool created = false;
            if (!EnsureGameplayInputMap(
                    candidate.CurrentProject().rootPath,
                    input,
                    created,
                    error))
            {
                error = "Could not prepare the project gameplay input-map: " + error;
                return false;
            }

            constexpr const char* declaration =
                "data:Content/Data/GameplayInput.renegade-input";
            auto alwaysInclude = candidate.CurrentProject().alwaysInclude;
            if (std::find(alwaysInclude.begin(), alwaysInclude.end(), declaration) ==
                alwaysInclude.end())
            {
                alwaysInclude.emplace_back(declaration);
                if (!candidate.SetAlwaysInclude(alwaysInclude))
                {
                    error = "Could not register the project gameplay input-map for packaging: " +
                        candidate.LastError();
                    return false;
                }
            }
            error.clear();
            return true;
        }

        void FailPending(std::string error, std::string warning)
        {
            pendingProject_ = {};
            hasPendingProject_ = false;
            pendingError_ = std::move(error);
            pendingWarning_ = std::move(warning);
        }

        void Stage(ProjectMetadata metadata, std::string warning)
        {
            pendingProject_ = std::move(metadata);
            hasPendingProject_ = true;
            pendingError_.clear();
            pendingWarning_ = std::move(warning);
        }

        ProjectMetadata pendingProject_;
        bool hasPendingProject_ = false;
        std::string pendingError_;
        std::string pendingWarning_;
    };
}
