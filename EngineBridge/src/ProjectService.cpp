#include "renegade/bridge/ProjectService.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <filesystem>
#include <system_error>

#include <wiConfig.h>
#include <wiHelper.h>

namespace
{
    namespace fs = std::filesystem;

    constexpr const char* ProjectFormat = "renegade-project";
    constexpr const char* DefaultStartupScene = "Content/Scenes/Main.wiscene";

    constexpr std::array<const char*, 35> DefaultProjectDirectories = {
        "Content/Scenes",
        "Content/Models",
        "Content/Prefabs",
        "Content/Materials",
        "Content/Textures",
        "Content/Audio/SFX",
        "Content/Audio/Music",
        "Content/Audio/Ambience",
        "Content/Audio/Dialogue",
        "Content/Audio/UI",
        "Content/Video",
        "Content/Vegetation",
        "Content/Characters",
        "Content/Player",
        "Content/Weapons",
        "Content/Projectiles",
        "Content/Particles",
        "Content/Scripts",
        "Content/UI",
        "Content/Data",
        "Content/Generated",
        "SourceAssets/Models",
        "SourceAssets/Textures",
        "SourceAssets/Audio",
        "SourceAssets/Video",
        "SourceAssets/Animation",
        "Saved/Autosaves",
        "Saved/Backups",
        "Saved/ImportLogs",
        "Saved/Thumbnails",
        "Saved/CrashReports",
        "Saved/EditorState",
        "Intermediate",
        "Builds/Windows",
        "Builds/Development",
    };

    std::string NormalizedAbsolutePath(const std::string& path)
    {
        return fs::absolute(fs::u8path(path)).lexically_normal().generic_u8string();
    }

    bool IsValidProjectName(const std::string& name)
    {
        if (name.empty() || name == "." || name == ".." ||
            name.back() == ' ' || name.back() == '.')
        {
            return false;
        }

        constexpr const char* invalidCharacters = "<>:\"/\\|?*";
        return std::none_of(
            name.begin(),
            name.end(),
            [invalidCharacters](const unsigned char character)
            {
                return character < 32 ||
                    std::strchr(
                        invalidCharacters,
                        static_cast<int>(character)) != nullptr;
            });
    }

    bool IsSafeRelativePath(const fs::path& path)
    {
        if (path.empty() || path.is_absolute())
        {
            return false;
        }

        return std::none_of(
            path.begin(),
            path.end(),
            [](const fs::path& part)
            {
                return part == "..";
            });
    }

    bool EnsureDefaultProjectDirectories(
        const fs::path& root,
        std::string& error)
    {
        std::error_code directoryError;
        for (const char* directory : DefaultProjectDirectories)
        {
            fs::create_directories(
                root / fs::u8path(directory),
                directoryError);
            if (directoryError)
            {
                error = "Could not initialise project folder '" +
                    std::string(directory) + "': " +
                    directoryError.message();
                return false;
            }
        }
        error.clear();
        return true;
    }
}

namespace renegade::bridge
{
    void ProjectService::Initialize(const std::string& stateFilePath)
    {
        stateFilePath_ = NormalizedAbsolutePath(stateFilePath);
        recentProjects_.clear();
        lastError_.clear();
        LoadRecents();
    }

    bool ProjectService::CreateProject(
        const std::string& parentDirectory,
        const std::string& projectName,
        const std::string& templateScenePath)
    {
        lastError_.clear();

        if (!IsValidProjectName(projectName))
        {
            lastError_ =
                "Project names cannot be empty or contain Windows filename characters.";
            return false;
        }
        if (parentDirectory.empty() || !fs::is_directory(fs::u8path(parentDirectory)))
        {
            lastError_ = "Choose an existing parent folder for the project.";
            return false;
        }
        if (templateScenePath.empty() ||
            !fs::is_regular_file(fs::u8path(templateScenePath)))
        {
            lastError_ = "The Renegade starter scene is unavailable.";
            return false;
        }

        try
        {
            const fs::path root =
                fs::absolute(fs::u8path(parentDirectory) / fs::u8path(projectName))
                    .lexically_normal();
            if (fs::exists(root) && !fs::is_empty(root))
            {
                lastError_ =
                    "A non-empty project folder already exists: " +
                    root.generic_u8string();
                return false;
            }

            const fs::path scenePath = root / fs::u8path(DefaultStartupScene);
            for (const char* directory : DefaultProjectDirectories)
            {
                fs::create_directories(root / fs::u8path(directory));
            }
            fs::copy_file(
                fs::u8path(templateScenePath),
                scenePath,
                fs::copy_options::overwrite_existing);

            ProjectMetadata metadata;
            metadata.formatVersion = CurrentFormatVersion;
            metadata.name = projectName;
            metadata.rootPath = root.generic_u8string();
            metadata.descriptorPath =
                (root / fs::u8path(projectName + ".renegade")).generic_u8string();
            metadata.startupScene = DefaultStartupScene;

            if (!WriteProject(metadata))
            {
                return false;
            }

            currentProject_ = metadata;
            hasProject_ = true;
            AddRecent(metadata);
            return true;
        }
        catch (const std::exception& exception)
        {
            lastError_ =
                std::string("Could not create the project: ") + exception.what();
            return false;
        }
    }

    bool ProjectService::OpenProject(const std::string& descriptorPath)
    {
        ProjectMetadata metadata;
        std::string error;
        if (!ReadProject(descriptorPath, metadata, error))
        {
            lastError_ = std::move(error);
            return false;
        }

        if (!EnsureDefaultProjectDirectories(
                fs::u8path(metadata.rootPath),
                error))
        {
            lastError_ = std::move(error);
            return false;
        }

        currentProject_ = metadata;
        hasProject_ = true;
        lastError_.clear();
        AddRecent(metadata);
        return true;
    }

    void ProjectService::CloseProject() noexcept
    {
        currentProject_ = {};
        hasProject_ = false;
        lastError_.clear();
    }

    bool ProjectService::HasProject() const noexcept
    {
        return hasProject_;
    }

    const ProjectMetadata& ProjectService::CurrentProject() const noexcept
    {
        return currentProject_;
    }

    std::string ProjectService::StartupScenePath() const
    {
        if (!hasProject_)
        {
            return {};
        }

        return (
            fs::u8path(currentProject_.rootPath) /
            fs::u8path(currentProject_.startupScene))
            .lexically_normal()
            .generic_u8string();
    }

    const std::vector<RecentProject>& ProjectService::RecentProjects() const noexcept
    {
        return recentProjects_;
    }

    const std::string& ProjectService::LastError() const noexcept
    {
        return lastError_;
    }

    bool ProjectService::ReadProject(
        const std::string& descriptorPath,
        ProjectMetadata& metadata,
        std::string& error) const
    {
        if (descriptorPath.empty())
        {
            error = "Choose a Renegade project file.";
            return false;
        }

        try
        {
            const fs::path descriptor =
                fs::absolute(fs::u8path(descriptorPath)).lexically_normal();
            if (!fs::is_regular_file(descriptor))
            {
                error = "Project file does not exist: " + descriptor.generic_u8string();
                return false;
            }

            wi::config::File projectFile;
            if (!projectFile.Open(descriptor.generic_u8string()))
            {
                error = "Could not read project file: " + descriptor.generic_u8string();
                return false;
            }
            if (projectFile.GetText("format") != ProjectFormat)
            {
                error = "The selected file is not a Renegade project.";
                return false;
            }

            const int version = projectFile.GetInt("version");
            if (version != static_cast<int>(CurrentFormatVersion))
            {
                error =
                    "Unsupported Renegade project version: " +
                    std::to_string(version);
                return false;
            }
            if (!projectFile.HasSection("project"))
            {
                error = "The project descriptor is missing its project section.";
                return false;
            }

            const auto& project = projectFile.GetSection("project");
            const std::string name = project.GetText("name");
            const fs::path startupScene =
                fs::u8path(project.GetText("startup_scene"));
            if (!IsValidProjectName(name) || !IsSafeRelativePath(startupScene))
            {
                error = "The project descriptor contains invalid project metadata.";
                return false;
            }

            const fs::path root = descriptor.parent_path();
            const fs::path startupPath = (root / startupScene).lexically_normal();
            if (!fs::is_regular_file(startupPath))
            {
                error =
                    "Project startup scene is missing: " +
                    startupPath.generic_u8string();
                return false;
            }

            metadata.formatVersion = static_cast<std::uint32_t>(version);
            metadata.name = name;
            metadata.descriptorPath = descriptor.generic_u8string();
            metadata.rootPath = root.generic_u8string();
            metadata.startupScene = startupScene.generic_u8string();
            error.clear();
            return true;
        }
        catch (const std::exception& exception)
        {
            error = std::string("Could not open the project: ") + exception.what();
            return false;
        }
    }

    bool ProjectService::WriteProject(const ProjectMetadata& metadata)
    {
        wi::config::File projectFile;
        projectFile.Open(metadata.descriptorPath);
        projectFile.Set("format", ProjectFormat);
        projectFile.Set("version", CurrentFormatVersion);
        auto& project = projectFile.GetSection("project");
        project.Set("name", metadata.name);
        project.Set("startup_scene", metadata.startupScene);
        projectFile.Commit();

        if (!wi::helper::FileExists(metadata.descriptorPath))
        {
            lastError_ =
                "Could not write project descriptor: " + metadata.descriptorPath;
            return false;
        }

        lastError_.clear();
        return true;
    }

    void ProjectService::AddRecent(const ProjectMetadata& metadata)
    {
        recentProjects_.erase(
            std::remove_if(
                recentProjects_.begin(),
                recentProjects_.end(),
                [&metadata](const RecentProject& recent)
                {
                    return recent.descriptorPath == metadata.descriptorPath;
                }),
            recentProjects_.end());

        recentProjects_.insert(
            recentProjects_.begin(),
            RecentProject{metadata.name, metadata.descriptorPath});
        if (recentProjects_.size() > MaximumRecentProjects)
        {
            recentProjects_.resize(MaximumRecentProjects);
        }
        PersistRecents();
    }

    void ProjectService::LoadRecents()
    {
        if (stateFilePath_.empty())
        {
            return;
        }

        wi::config::File state;
        if (!state.Open(stateFilePath_) || !state.HasSection("recent_projects"))
        {
            return;
        }

        const auto& recent = state.GetSection("recent_projects");
        const int count = std::clamp(
            recent.GetInt("count"),
            0,
            static_cast<int>(MaximumRecentProjects));
        for (int index = 0; index < count; ++index)
        {
            ProjectMetadata metadata;
            std::string error;
            if (ReadProject(
                    recent.GetText(("project_" + std::to_string(index)).c_str()),
                    metadata,
                    error))
            {
                recentProjects_.push_back(
                    RecentProject{metadata.name, metadata.descriptorPath});
            }
        }

        lastError_.clear();
    }

    void ProjectService::SetEditorPreference(
        const std::string& key,
        const bool value)
    {
        if (stateFilePath_.empty() || key.empty())
        {
            return;
        }

        try
        {
            fs::create_directories(fs::u8path(stateFilePath_).parent_path());
            wi::config::File state;
            state.Open(stateFilePath_);
            state.GetSection("editor").Set(key.c_str(), value);
            state.Commit();
        }
        catch (const std::exception&)
        {
            // A read-only settings location must not break the editor.
            // The preference simply will not survive this session.
        }
    }

    bool ProjectService::GetEditorPreference(
        const std::string& key,
        const bool fallback) const
    {
        if (stateFilePath_.empty() || key.empty())
        {
            return fallback;
        }

        wi::config::File state;
        if (!state.Open(stateFilePath_) || !state.HasSection("editor"))
        {
            return fallback;
        }

        const auto& editor = state.GetSection("editor");
        if (!editor.Has(key.c_str()))
        {
            return fallback;
        }

        return editor.GetBool(key.c_str());
    }

    void ProjectService::PersistRecents()
    {
        if (stateFilePath_.empty())
        {
            return;
        }

        try
        {
            fs::create_directories(fs::u8path(stateFilePath_).parent_path());
            wi::config::File state;
            state.Open(stateFilePath_);
            auto& recent = state.GetSection("recent_projects");
            recent.Set("count", static_cast<int>(recentProjects_.size()));
            for (std::size_t index = 0; index < recentProjects_.size(); ++index)
            {
                recent.Set(
                    ("project_" + std::to_string(index)).c_str(),
                    recentProjects_[index].descriptorPath);
            }
            state.Commit();
        }
        catch (const std::exception&)
        {
            // A read-only settings location must not prevent a project opening.
        }
    }
}
