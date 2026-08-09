#include "renegade/bridge/ProjectService.h"
#include "renegade/bridge/DependencyService.h"

#include "renegade/bridge/IdentityService.h"
#include "renegade/bridge/ProjectDocumentTransaction.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <system_error>
#include <utility>
#include <vector>

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

        constexpr const char* invalidCharacters = "<>:\"/\\|?*#;";
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

    constexpr std::uint32_t AlwaysIncludeFormatVersion = 1;
    constexpr std::uint32_t MaximumAlwaysIncludeEntries = 4096;

    std::string EncodeProjectConfigValue(const std::string& value)
    {
        constexpr char Hex[] = "0123456789ABCDEF";
        std::string encoded;
        encoded.reserve(value.size());
        for (const unsigned char character : value)
        {
            const bool safe =
                (character >= 'a' && character <= 'z') ||
                (character >= 'A' && character <= 'Z') ||
                (character >= '0' && character <= '9') ||
                character == '/' || character == '.' || character == '_' ||
                character == '-';
            if (safe)
            {
                encoded.push_back(static_cast<char>(character));
            }
            else
            {
                encoded.push_back('%');
                encoded.push_back(Hex[(character >> 4) & 0x0F]);
                encoded.push_back(Hex[character & 0x0F]);
            }
        }
        return encoded;
    }

    bool DecodeProjectConfigValue(
        const std::string& encoded,
        std::string& decoded)
    {
        const auto nibble = [](const char character) -> int
        {
            if (character >= '0' && character <= '9') return character - '0';
            if (character >= 'a' && character <= 'f') return character - 'a' + 10;
            if (character >= 'A' && character <= 'F') return character - 'A' + 10;
            return -1;
        };

        decoded.clear();
        decoded.reserve(encoded.size());
        for (std::size_t index = 0; index < encoded.size(); ++index)
        {
            if (encoded[index] != '%')
            {
                decoded.push_back(encoded[index]);
                continue;
            }
            if (index + 2 >= encoded.size())
                return false;
            const int high = nibble(encoded[index + 1]);
            const int low = nibble(encoded[index + 2]);
            if (high < 0 || low < 0)
                return false;
            decoded.push_back(static_cast<char>((high << 4) | low));
            index += 2;
        }
        return true;
    }

    bool ParseAlwaysIncludeDeclaration(
        const std::string& entry,
        renegade::bridge::DependencyClass& dependencyClass,
        std::string& path)
    {
        const auto separator = entry.find(':');
        if (separator == std::string::npos ||
            !renegade::bridge::TryParseDependencyClassName(
                entry.substr(0, separator), dependencyClass))
        {
            return false;
        }
        path = entry.substr(separator + 1);
        return IsSafeRelativePath(fs::u8path(path));
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

    bool ReadFileBytes(
        const fs::path& path,
        std::vector<std::uint8_t>& bytes,
        std::string& error)
    {
        bytes.clear();
        std::ifstream stream(path, std::ios::binary);
        if (!stream)
        {
            error = "Could not read project document: " +
                path.generic_u8string();
            return false;
        }
        stream.seekg(0, std::ios::end);
        const std::streamoff size = stream.tellg();
        if (size < 0)
        {
            error = "Could not inspect project document size: " +
                path.generic_u8string();
            return false;
        }
        stream.seekg(0, std::ios::beg);
        bytes.resize(static_cast<std::size_t>(size));
        if (!bytes.empty())
        {
            stream.read(
                reinterpret_cast<char*>(bytes.data()),
                static_cast<std::streamsize>(bytes.size()));
        }
        if (!stream && !bytes.empty())
        {
            error = "Could not read complete project document: " +
                path.generic_u8string();
            bytes.clear();
            return false;
        }
        error.clear();
        return true;
    }

    bool FileMatchesBytes(
        const std::string& path,
        const std::vector<std::uint8_t>& expected,
        std::string& error)
    {
        std::vector<std::uint8_t> actual;
        if (!ReadFileBytes(fs::u8path(path), actual, error))
        {
            return false;
        }
        if (actual != expected)
        {
            error = "The project document does not match its requested bytes.";
            return false;
        }
        error.clear();
        return true;
    }

    std::vector<std::uint8_t> SerializeProjectDescriptor(
        const renegade::bridge::ProjectMetadata& metadata)
    {
        std::ostringstream stream;
        stream << "format = " << ProjectFormat << '\n';
        stream << "version = "
               << renegade::bridge::ProjectService::CurrentFormatVersion
               << "\n\n";
        stream << "[project]\n";
        stream << "project_id = " << metadata.projectId << '\n';
        stream << "name = " << metadata.name << '\n';
        stream << "startup_scene = " << metadata.startupScene << '\n';
        stream << "startup_flow_id = " << metadata.startupFlowId << '\n';
        stream << "startup_flow = " << metadata.startupFlow << '\n';
        stream << "startup_screen_id = " << metadata.startupScreenId << '\n';
        stream << "startup_screen = " << metadata.startupScreen << '\n';
        if (!metadata.alwaysInclude.empty())
        {
            stream << "\n[dependencies]\n";
            stream << "always_include_format = "
                   << AlwaysIncludeFormatVersion << '\n';
            stream << "always_include_count = "
                   << metadata.alwaysInclude.size() << '\n';
            for (std::size_t index = 0; index < metadata.alwaysInclude.size(); ++index)
            {
                renegade::bridge::DependencyClass dependencyClass{};
                std::string path;
                const bool parsed = ParseAlwaysIncludeDeclaration(
                    metadata.alwaysInclude[index], dependencyClass, path);
                (void)parsed; // WriteProject validates before serialization.
                stream << "always_include_" << index << "_class = "
                       << renegade::bridge::DependencyClassName(dependencyClass)
                       << '\n';
                stream << "always_include_" << index << "_path = "
                       << EncodeProjectConfigValue(path) << '\n';
            }
        }
        const std::string text = stream.str();
        return std::vector<std::uint8_t>(text.begin(), text.end());
    }

    bool ProjectMetadataMatches(
        const renegade::bridge::ProjectMetadata& expected,
        const renegade::bridge::ProjectMetadata& actual)
    {
        return actual.formatVersion ==
                renegade::bridge::ProjectService::CurrentFormatVersion &&
            actual.projectId == expected.projectId &&
            actual.name == expected.name &&
            actual.startupScene == expected.startupScene &&
            actual.startupFlowId == expected.startupFlowId &&
            actual.startupFlow == expected.startupFlow &&
            actual.startupScreenId == expected.startupScreenId &&
            actual.startupScreen == expected.startupScreen &&
            actual.alwaysInclude == expected.alwaysInclude;
    }

    fs::path ProjectTransactionDirectory(const fs::path& root)
    {
        return root / "Intermediate" / "Transactions";
    }

    fs::path ProjectDescriptorBackupPath(const fs::path& descriptor)
    {
        return descriptor.parent_path() /
            fs::u8path(
                descriptor.stem().generic_u8string() + ".bak" +
                descriptor.extension().generic_u8string());
    }

    void AppendWarning(std::string& warning, std::string addition)
    {
        if (addition.empty())
        {
            return;
        }
        if (!warning.empty())
        {
            warning += " ";
        }
        warning += std::move(addition);
    }
}

namespace renegade::bridge
{
    ProjectDependencyReader MakeProjectDependencyReader()
    {
        return [](const std::string& path,
            ProjectDependencyDocument& document, std::string& error)
        {
            ProjectService projects;
            ProjectMetadata metadata;
            if (!projects.InspectProjectForDependencies(path, metadata, error))
                return false;
            document.projectId = metadata.projectId;
            document.startupScene = metadata.startupScene;
            document.startupFlow = metadata.startupFlow;
            document.startupScreen = metadata.startupScreen;
            document.alwaysInclude.clear();
            document.alwaysInclude.reserve(metadata.alwaysInclude.size());
            for (const auto& entry : metadata.alwaysInclude)
            {
                DependencyCandidate candidate;
                DependencyClass parsedClass{};
                std::string declaredPath;
                if (!ParseAlwaysIncludeDeclaration(
                        entry, parsedClass, declaredPath))
                {
                    error = "Validated project metadata contained an invalid "
                        "Always Include declaration.";
                    return false;
                }
                candidate.declaredPath = std::move(declaredPath);
                candidate.dependencyClass = parsedClass;
                candidate.requirement = DependencyRequirement::Required;
                document.alwaysInclude.push_back(std::move(candidate));
            }
            error.clear();
            return true;
        };
    }
    void ProjectService::Initialize(const std::string& stateFilePath)
    {
        stateFilePath_ = NormalizedAbsolutePath(stateFilePath);
        recentProjects_.clear();
        lastError_.clear();
        lastWarning_.clear();
        LoadRecents();
    }

    bool ProjectService::CreateProject(
        const std::string& parentDirectory,
        const std::string& projectName,
        const std::string& templateScenePath)
    {
        lastError_.clear();
        lastWarning_.clear();

        if (!IsValidProjectName(projectName))
        {
            lastError_ =
                "Project names cannot be empty or contain unsafe filename or project-file characters.";
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
            metadata.projectId = GenerateStableId();
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
        lastError_.clear();
        lastWarning_.clear();

        if (!descriptorPath.empty())
        {
            try
            {
                const fs::path root =
                    fs::absolute(fs::u8path(descriptorPath))
                        .lexically_normal()
                        .parent_path();
                if (!RecoverProjectTransactions(root.generic_u8string()))
                {
                    return false;
                }
            }
            catch (const std::exception& exception)
            {
                lastError_ = "Could not prepare project transaction recovery: " +
                    std::string(exception.what());
                return false;
            }
        }

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

        // The additive v1 identity field is migrated only through mutable
        // Studio open. WriteProject() now provides validation, rollback,
        // recovery evidence and a retained previous-descriptor backup.
        if (metadata.projectId.empty())
        {
            metadata.projectId = GenerateStableId();
            if (!WriteProject(metadata))
            {
                return false;
            }
        }

        currentProject_ = metadata;
        hasProject_ = true;
        lastError_.clear();
        AddRecent(metadata);
        return true;
    }

    bool ProjectService::InspectProject(
        const std::string& descriptorPath,
        ProjectMetadata& metadata,
        std::string& error) const
    {
        if (!ReadProject(descriptorPath, metadata, error))
        {
            return false;
        }
        if (metadata.projectId.empty())
        {
            error = "The project descriptor is missing its stable project ID. "
                "Open it in Renegade Studio to migrate it.";
            return false;
        }
        error.clear();
        return true;
    }

    bool ProjectService::InspectProjectForDependencies(
        const std::string& descriptorPath,
        ProjectMetadata& metadata,
        std::string& error) const
    {
        if (!ReadProject(descriptorPath, metadata, error, false))
            return false;
        if (metadata.projectId.empty())
        {
            error = "The project descriptor is missing its stable project ID. "
                "Open it in Renegade Studio to migrate it before extraction.";
            return false;
        }
        error.clear();
        return true;
    }

    void ProjectService::CloseProject() noexcept
    {
        currentProject_ = {};
        hasProject_ = false;
        lastError_.clear();
        lastWarning_.clear();
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

    const std::string& ProjectService::LastWarning() const noexcept
    {
        return lastWarning_;
    }

    bool ProjectService::ReadProject(
        const std::string& descriptorPath,
        ProjectMetadata& metadata,
        std::string& error,
        const bool requireStartupScene) const
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
            const std::string projectId = project.GetText("project_id");
            const std::string name = project.GetText("name");
            const fs::path startupScene =
                fs::u8path(project.GetText("startup_scene"));
            const std::string startupFlowId =
                project.GetText("startup_flow_id");
            const fs::path startupFlow =
                fs::u8path(project.GetText("startup_flow"));
            const std::string startupScreenId =
                project.GetText("startup_screen_id");
            const fs::path startupScreen =
                fs::u8path(project.GetText("startup_screen"));
            if (!projectId.empty() && !IsValidStableId(projectId))
            {
                error = "The project descriptor contains a malformed project ID.";
                return false;
            }
            const bool hasStartupFlowId = !startupFlowId.empty();
            const bool hasStartupFlowHint = !startupFlow.empty();
            const bool hasStartupScreenId = !startupScreenId.empty();
            const bool hasStartupScreenHint = !startupScreen.empty();
            if (!IsValidProjectName(name) ||
                !IsSafeRelativePath(startupScene) ||
                (hasStartupFlowId != hasStartupFlowHint) ||
                (hasStartupFlowId && !IsValidStableId(startupFlowId)) ||
                (hasStartupFlowHint && !IsSafeRelativePath(startupFlow)) ||
                (hasStartupScreenId != hasStartupScreenHint) ||
                (hasStartupScreenId && !IsValidStableId(startupScreenId)) ||
                (hasStartupScreenHint && !IsSafeRelativePath(startupScreen)))
            {
                error = "The project descriptor contains invalid project metadata.";
                return false;
            }

            std::vector<std::string> alwaysInclude;
            if (projectFile.HasSection("dependencies"))
            {
                const auto& dependencies = projectFile.GetSection("dependencies");
                if (dependencies.Has("always_include_format"))
                {
                    const std::uint32_t format =
                        dependencies.GetUint("always_include_format");
                    const std::uint32_t count =
                        dependencies.GetUint("always_include_count");
                    if (!dependencies.Has("always_include_count") ||
                        format != AlwaysIncludeFormatVersion ||
                        count > MaximumAlwaysIncludeEntries)
                    {
                        error = "The project descriptor contains an invalid "
                            "Always Include format or entry count.";
                        return false;
                    }
                    alwaysInclude.reserve(count);
                    for (std::uint32_t index = 0; index < count; ++index)
                    {
                        const std::string prefix =
                            "always_include_" + std::to_string(index);
                        const std::string className =
                            dependencies.GetText((prefix + "_class").c_str());
                        const std::string encodedPath =
                            dependencies.GetText((prefix + "_path").c_str());
                        std::string path;
                        renegade::bridge::DependencyClass parsedClass{};
                        if (!renegade::bridge::TryParseDependencyClassName(
                                className, parsedClass) ||
                            !DecodeProjectConfigValue(encodedPath, path) ||
                            !IsSafeRelativePath(fs::u8path(path)))
                        {
                            error = "The project descriptor contains an invalid "
                                "Always Include entry at index " +
                                std::to_string(index) + ".";
                            return false;
                        }
                        alwaysInclude.push_back(className + ":" + path);
                    }
                }
                else
                {
                    // Backward compatibility for the short-lived Gate 5
                    // comma-array format merged in PR #29. New writes use
                    // numbered, encoded fields because wi::config arrays
                    // cannot escape commas, '#' or ';' in valid filenames.
                    for (const auto& entry :
                        dependencies.GetTextArray("always_include"))
                    {
                        renegade::bridge::DependencyClass parsedClass{};
                        std::string path;
                        if (!ParseAlwaysIncludeDeclaration(
                                entry, parsedClass, path))
                        {
                            error = "The project descriptor contains an invalid "
                                "legacy always-include dependency declaration: " +
                                entry;
                            return false;
                        }
                        alwaysInclude.push_back(entry);
                    }
                }
            }

            const fs::path root = descriptor.parent_path();
            const fs::path startupPath = (root / startupScene).lexically_normal();
            if (requireStartupScene && !fs::is_regular_file(startupPath))
            {
                error =
                    "Project startup scene is missing: " +
                    startupPath.generic_u8string();
                return false;
            }
            metadata.formatVersion = static_cast<std::uint32_t>(version);
            metadata.projectId = projectId;
            metadata.name = name;
            metadata.descriptorPath = descriptor.generic_u8string();
            metadata.rootPath = root.generic_u8string();
            metadata.startupScene = startupScene.generic_u8string();
            metadata.startupFlowId = startupFlowId;
            metadata.startupFlow = startupFlow.generic_u8string();
            metadata.startupScreenId = startupScreenId;
            metadata.startupScreen = startupScreen.generic_u8string();
            metadata.alwaysInclude = std::move(alwaysInclude);
            error.clear();
            return true;
        }
        catch (const std::exception& exception)
        {
            error = std::string("Could not open the project: ") + exception.what();
            return false;
        }
    }

    bool ProjectService::RecoverProjectTransactions(
        const std::string& projectRoot)
    {
        const fs::path root = fs::u8path(projectRoot).lexically_normal();
        const fs::path directory = ProjectTransactionDirectory(root);
        std::error_code error;
        if (!fs::exists(directory, error))
        {
            if (error)
            {
                lastError_ = "Could not inspect project transaction recovery: " +
                    error.message();
                return false;
            }
            return true;
        }
        if (!fs::is_directory(directory, error) || error)
        {
            lastError_ = "The project transaction location is not a directory: " +
                directory.generic_u8string();
            return false;
        }

        std::vector<fs::path> journals;
        for (fs::directory_iterator iterator(directory, error);
            !error && iterator != fs::directory_iterator();
            iterator.increment(error))
        {
            if (!iterator->is_regular_file())
            {
                continue;
            }
            const std::string name =
                iterator->path().filename().generic_u8string();
            if (name.rfind(".renegade-transaction-", 0) == 0 &&
                iterator->path().extension() == ".journal")
            {
                journals.push_back(iterator->path());
            }
        }
        if (error)
        {
            lastError_ = "Could not enumerate project transaction recovery: " +
                error.message();
            return false;
        }
        std::sort(journals.begin(), journals.end());

        ProjectDocumentTransaction transaction;
        ProjectDocumentTransactionOptions options;
        options.journalDirectory = directory.generic_u8string();
        options.allowedRoot = root.generic_u8string();
        std::size_t recoveredCount = 0;
        for (const auto& journal : journals)
        {
            const auto result = transaction.Recover(
                journal.generic_u8string(),
                options);
            if (!result.success)
            {
                lastError_ = "Project transaction recovery failed [" +
                    result.code + "]: " + result.message;
                return false;
            }
            ++recoveredCount;
        }
        if (recoveredCount > 0)
        {
            AppendWarning(
                lastWarning_,
                "Recovered " + std::to_string(recoveredCount) +
                    " interrupted project document transaction(s).");
        }
        return true;
    }

    bool ProjectService::WriteProject(const ProjectMetadata& metadata)
    {
        lastError_.clear();
        if (!IsValidStableId(metadata.projectId))
        {
            lastError_ = "Could not write a project without a valid stable project ID.";
            return false;
        }
        if (!IsValidProjectName(metadata.name) ||
            !IsSafeRelativePath(fs::u8path(metadata.startupScene)))
        {
            lastError_ = "Could not write invalid project metadata.";
            return false;
        }

        const bool hasStartupFlowId = !metadata.startupFlowId.empty();
        const bool hasStartupFlowHint = !metadata.startupFlow.empty();
        const bool hasStartupScreenId = !metadata.startupScreenId.empty();
        const bool hasStartupScreenHint = !metadata.startupScreen.empty();
        if (hasStartupFlowId != hasStartupFlowHint ||
            (hasStartupFlowId && !IsValidStableId(metadata.startupFlowId)) ||
            (hasStartupFlowHint &&
                !IsSafeRelativePath(fs::u8path(metadata.startupFlow))) ||
            hasStartupScreenId != hasStartupScreenHint ||
            (hasStartupScreenId &&
                !IsValidStableId(metadata.startupScreenId)) ||
            (hasStartupScreenHint &&
                !IsSafeRelativePath(fs::u8path(metadata.startupScreen))))
        {
            lastError_ = "Could not write an invalid startup Flow or Runtime screen reference.";
            return false;
        }

        if (metadata.alwaysInclude.size() > MaximumAlwaysIncludeEntries)
        {
            lastError_ = "Could not write more than " +
                std::to_string(MaximumAlwaysIncludeEntries) +
                " Always Include dependency declarations.";
            return false;
        }
        for (const auto& entry : metadata.alwaysInclude)
        {
            DependencyClass parsedClass{};
            std::string path;
            if (!ParseAlwaysIncludeDeclaration(entry, parsedClass, path))
            {
                lastError_ = "Could not write an invalid always-include "
                    "dependency declaration: " + entry;
                return false;
            }
        }

        std::error_code pathError;
        const fs::path descriptor =
            fs::absolute(fs::u8path(metadata.descriptorPath), pathError)
                .lexically_normal();
        if (pathError || descriptor.filename().empty())
        {
            lastError_ = "Could not resolve the project descriptor path: " +
                pathError.message();
            return false;
        }
        const fs::path root = descriptor.parent_path();

        std::vector<std::uint8_t> previousContent;
        const bool replacingExisting = fs::exists(descriptor, pathError);
        if (pathError)
        {
            lastError_ = "Could not inspect the project descriptor: " +
                pathError.message();
            return false;
        }
        if (replacingExisting &&
            !ReadFileBytes(descriptor, previousContent, lastError_))
        {
            return false;
        }

        const std::vector<std::uint8_t> requestedContent =
            SerializeProjectDescriptor(metadata);
        std::vector<ProjectDocumentWrite> writes;
        writes.reserve(replacingExisting ? 2u : 1u);

        // The retained last-good descriptor participates in the same
        // multi-file transaction as the new descriptor. A backup failure
        // therefore leaves the live descriptor untouched rather than becoming
        // a post-save warning or a crash window between two transactions.
        if (replacingExisting && previousContent != requestedContent)
        {
            ProjectDocumentWrite backupWrite;
            backupWrite.destinationPath =
                ProjectDescriptorBackupPath(descriptor).generic_u8string();
            backupWrite.content = previousContent;
            backupWrite.validator = [previousContent](
                const std::string& path,
                std::string& error)
            {
                return FileMatchesBytes(path, previousContent, error);
            };
            writes.push_back(std::move(backupWrite));
        }

        ProjectDocumentWrite descriptorWrite;
        descriptorWrite.destinationPath = descriptor.generic_u8string();
        descriptorWrite.content = requestedContent;
        descriptorWrite.validator = [this, metadata](
            const std::string& path,
            std::string& error)
        {
            ProjectMetadata validated;
            if (!ReadProject(path, validated, error))
            {
                return false;
            }
            if (!ProjectMetadataMatches(metadata, validated))
            {
                error = "The staged project descriptor did not round-trip exactly.";
                return false;
            }
            error.clear();
            return true;
        };
        writes.push_back(std::move(descriptorWrite));

        ProjectDocumentTransactionOptions options;
        options.journalDirectory =
            ProjectTransactionDirectory(root).generic_u8string();
        options.allowedRoot = root.generic_u8string();

        ProjectDocumentTransaction transaction;
        const auto result = transaction.Execute(std::move(writes), options);
        if (!result.success)
        {
            lastError_ = "Project descriptor transaction failed [" +
                result.code + "]: " + result.message;
            return false;
        }
        if (result.recoveryRequired)
        {
            AppendWarning(
                lastWarning_,
                "Project descriptor committed, but transaction cleanup remains pending: " +
                    result.message);
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
