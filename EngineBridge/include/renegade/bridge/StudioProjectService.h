#pragma once

#include <filesystem>
#include <string>
#include <utility>

#include "renegade/bridge/ProjectService.h"

namespace renegade::bridge
{
    // Studio-only transactional facade over ProjectService.
    //
    // ProjectService remains the authoritative project persistence service.
    // This facade prevents Studio from changing its active project identity or
    // Recent Projects list until the requested startup scene has been prepared
    // successfully. The candidate project is validated/migrated in an isolated
    // ProjectService instance first; CommitPendingProject() performs the real
    // adoption only at the scene replacement boundary.
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
                pendingProject_ = {};
                hasPendingProject_ = false;
                pendingError_ = candidate.LastError();
                pendingWarning_ = candidate.LastWarning();
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
                pendingProject_ = {};
                hasPendingProject_ = false;
                pendingError_ = candidate.LastError();
                pendingWarning_ = candidate.LastWarning();
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
