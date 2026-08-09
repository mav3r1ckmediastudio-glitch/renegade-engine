#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace renegade::bridge
{
    struct ProjectMetadata
    {
        std::uint32_t formatVersion = 1;
        std::string projectId;
        std::string name;
        std::string descriptorPath;
        std::string rootPath;
        std::string startupScene = "Content/Scenes/Main.wiscene";
        // Optional Renegade-owned Story Flow root. The stable document ID is
        // authoritative; startupFlow is a project-relative location hint.
        // Projects without either keep the LP01 startup-scene path.
        std::string startupFlowId;
        std::string startupFlow;
        // Optional Renegade-owned Runtime screen root. The stable document ID
        // is authoritative; startupScreen is a project-relative location hint.
        // Projects without either retain the LP01/LP02 immediate-start path.
        std::string startupScreenId;
        std::string startupScreen;
        // Gate 5: project-relative paths declared to remain in the
        // dependency closure regardless of whether any scene, flow or screen
        // document currently references them. Each entry is stored as it was
        // validated on read/write: "<dependency_class_name>:<project_relative_path>".
        // Absent from a project descriptor entirely means no declarations,
        // so projects written before this field existed keep loading
        // unchanged. See DependencyClassName/TryParseDependencyClassName in
        // DependencyService.h for the recognized class names.
        std::vector<std::string> alwaysInclude;
    };

    struct RecentProject
    {
        std::string name;
        std::string descriptorPath;
    };

    class ProjectService
    {
    public:
        static constexpr std::uint32_t CurrentFormatVersion = 1;
        static constexpr std::size_t MaximumRecentProjects = 8;

        void Initialize(const std::string& stateFilePath);
        bool CreateProject(
            const std::string& parentDirectory,
            const std::string& projectName,
            const std::string& templateScenePath);
        bool OpenProject(const std::string& descriptorPath);

        // Read-only descriptor validation for Runtime/bootstrap tooling.
        // Unlike OpenProject(), this does not backfill folders, update recents
        // or mutate the active editor project. Legacy descriptors without a
        // stable project ID must first be opened by Studio for migration.
        [[nodiscard]] bool InspectProject(
            const std::string& descriptorPath,
            ProjectMetadata& metadata,
            std::string& error) const;
        [[nodiscard]] bool InspectProjectForDependencies(
            const std::string& descriptorPath,
            ProjectMetadata& metadata,
            std::string& error) const;

        void CloseProject() noexcept;

        [[nodiscard]] bool HasProject() const noexcept;
        [[nodiscard]] const ProjectMetadata& CurrentProject() const noexcept;
        [[nodiscard]] std::string StartupScenePath() const;
        [[nodiscard]] const std::vector<RecentProject>& RecentProjects() const noexcept;
        [[nodiscard]] const std::string& LastError() const noexcept;
        [[nodiscard]] const std::string& LastWarning() const noexcept;

        // Editor preferences persist beside the recent-project registry rather
        // than in a scene. They describe how the creator likes Studio to
        // behave, not what a project contains, so they must never reach a
        // WISCENE or the .renegade descriptor.
        void SetEditorPreference(const std::string& key, bool value);
        [[nodiscard]] bool GetEditorPreference(
            const std::string& key,
            bool fallback) const;

    private:
        bool ReadProject(
            const std::string& descriptorPath,
            ProjectMetadata& metadata,
            std::string& error,
            bool requireStartupScene = true) const;
        bool WriteProject(const ProjectMetadata& metadata);
        bool RecoverProjectTransactions(const std::string& projectRoot);
        void AddRecent(const ProjectMetadata& metadata);
        void LoadRecents();
        void PersistRecents();

        ProjectMetadata currentProject_;
        std::vector<RecentProject> recentProjects_;
        std::string stateFilePath_;
        std::string lastError_;
        std::string lastWarning_;
        bool hasProject_ = false;
    };
}
