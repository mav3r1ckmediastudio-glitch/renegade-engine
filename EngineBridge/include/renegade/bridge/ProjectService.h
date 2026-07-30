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
        std::string name;
        std::string descriptorPath;
        std::string rootPath;
        std::string startupScene = "Content/Scenes/Main.wiscene";
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
        void CloseProject() noexcept;

        [[nodiscard]] bool HasProject() const noexcept;
        [[nodiscard]] const ProjectMetadata& CurrentProject() const noexcept;
        [[nodiscard]] std::string StartupScenePath() const;
        [[nodiscard]] const std::vector<RecentProject>& RecentProjects() const noexcept;
        [[nodiscard]] const std::string& LastError() const noexcept;

    private:
        bool ReadProject(
            const std::string& descriptorPath,
            ProjectMetadata& metadata,
            std::string& error) const;
        bool WriteProject(const ProjectMetadata& metadata);
        void AddRecent(const ProjectMetadata& metadata);
        void LoadRecents();
        void PersistRecents();

        ProjectMetadata currentProject_;
        std::vector<RecentProject> recentProjects_;
        std::string stateFilePath_;
        std::string lastError_;
        bool hasProject_ = false;
    };
}
