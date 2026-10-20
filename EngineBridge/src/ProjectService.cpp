#include <map>
#include <set>
#include "renegade/bridge/GameplayAttributeDocumentService.h"
#include "renegade/bridge/ProjectService.h"
#include "renegade/bridge/ProjectUiScaffoldService.h"
#include "renegade/bridge/ScreenPerformanceBudget.h"
#include "renegade/bridge/HudSelection.h"
#include "renegade/bridge/ScreenService.h"
#include "renegade/bridge/ScreenLibraryService.h"
#include "renegade/bridge/DependencyService.h"

#include "renegade/bridge/AssetRegistryService.h"
#include "renegade/bridge/FlowService.h"
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
    constexpr const char* DefaultStartupFlow =
        "Content/StoryFlow/Main.renegade-flow";

    constexpr std::array<const char*, 36> DefaultProjectDirectories = {
        "Content/Scenes",
        "Content/StoryFlow",
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
        stream << "attribute_document_id = " << metadata.attributeDocumentId << '\n';
        stream << "attribute_document = " << metadata.attributeDocument << '\n';
        stream << "default_hud_id = " << metadata.defaultHudId << '\n';
        stream << "default_hud = " << metadata.defaultHud << '\n';
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
                (void)parsed;
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
            actual.defaultHudId == expected.defaultHudId && actual.defaultHud == expected.defaultHud &&
            actual.attributeDocumentId == expected.attributeDocumentId && actual.attributeDocument == expected.attributeDocument &&
            actual.alwaysInclude == expected.alwaysInclude;
    }

    bool PatchProjectAttributeReference(const std::vector<std::uint8_t>& input,
        const std::string& id,const std::string& hint,std::vector<std::uint8_t>& output,std::string& error)
    {
        const std::string original(input.begin(),input.end());
        const std::string ending=original.find("\r\n")!=std::string::npos?"\r\n":"\n";
        const std::map<std::string,std::string> values{{"attribute_document_id",id},{"attribute_document",hint}};
        std::set<std::string> seen;std::string result;bool inside=false,found=false;
        const auto trim=[](std::string text){
            const auto a=text.find_first_not_of(" \t\r\n");if(a==std::string::npos)return std::string{};
            return text.substr(a,text.find_last_not_of(" \t\r\n")-a+1);
        };
        const auto missing=[&](){
            for(const auto& entry:values)if(!seen.count(entry.first))
            {if(!result.empty()&&result.back()!='\n')result+=ending;result+=entry.first+" = "+entry.second+ending;seen.insert(entry.first);}
        };
        for(std::size_t pos=0;pos<original.size();)
        {
            const auto end=original.find('\n',pos);
            const auto length=end==std::string::npos?original.size()-pos:end-pos+1;
            const auto line=original.substr(pos,length);auto text=trim(line);
            if(pos==0&&text.compare(0,3,"\xEF\xBB\xBF")==0)text.erase(0,3);
            if(!text.empty()&&text.front()=='['&&text.find(']')!=std::string::npos)
            {
                if(inside)missing();
                const auto section=text.substr(0,text.find(']')+1);inside=section=="[project]";
                if(inside&&found){error="Duplicate project sections prevent a safe attribute-reference update.";return false;}
                found=found||inside;
            }
            bool replaced=false;
            if(inside)
            {
                const auto equal=text.find('=');
                if(equal!=std::string::npos)
                {
                    const auto key=trim(text.substr(0,equal));const auto value=values.find(key);
                    if(value!=values.end())
                    {
                        if(!seen.insert(key).second){error="Duplicate project attribute-reference keys require repair.";return false;}
                        result+=key+" = "+value->second;
                        if(!line.empty()&&line.back()=='\n')result+=line.size()>1&&line[line.size()-2]=='\r'?"\r\n":"\n";
                        replaced=true;
                    }
                }
            }
            if(!replaced)result+=line;pos+=length;
        }
        if(inside)missing();
        if(!found){error="Project section is missing; no descriptor bytes were changed.";return false;}
        output.assign(result.begin(),result.end());error.clear();return true;
    }

    bool ValidateAttributeReference(const std::string& id,const std::string& hint,std::string& error)
    {
        const auto path=fs::u8path(hint);
        if(!renegade::bridge::IsValidStableId(id)||!IsSafeRelativePath(path)||path.empty()||
            *path.begin()!="Content"||path.extension()!=".renegade-attributes"||
            hint.find_first_of("\r\n")!=std::string::npos||hint.find(char(0))!=std::string::npos)
        {error="Gameplay attribute reference requires a stable identity and Content-relative .renegade-attributes path.";return false;}
        return true;
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

    bool EnsureProjectAssetRegistry(
        const fs::path& root,
        const std::string& projectId,
        std::string& warning,
        std::string& error)
    {
        using namespace renegade::bridge;

        if (!IsValidStableId(projectId))
        {
            error = "Could not initialise the project asset registry without a valid project ID.";
            return false;
        }

        const fs::path registryPath = root / AssetRegistryDocumentName;
        std::error_code pathError;
        const bool exists = fs::exists(registryPath, pathError);
        if (pathError)
        {
            error = "Could not inspect the project asset registry: " +
                pathError.message();
            return false;
        }

        if (exists)
        {
            AssetRegistry existing;
            if (!ReadAssetRegistry(
                    root.generic_u8string(), projectId, existing, error))
            {
                error = "Could not validate the project asset registry: " + error;
                return false;
            }
            error.clear();
            return true;
        }

        AssetRegistry registry;
        registry.projectId = projectId;
        registry.schemaVersion = AssetRegistry::CurrentSchemaVersion;

        AssetRegistryPersistenceOptions options;
        options.transactionId = "project-registry-bootstrap-" + GenerateStableId();
        const auto written = WriteAssetRegistry(
            root.generic_u8string(), registry, std::move(options));
        if (!written.success || !written.committed)
        {
            error = "Could not initialise the project asset registry";
            if (!written.code.empty())
                error += " [" + written.code + "]";
            if (!written.message.empty())
                error += ": " + written.message;
            return false;
        }

        AssetRegistry verified;
        if (!ReadAssetRegistry(
                root.generic_u8string(), projectId, verified, error))
        {
            error = "Project asset registry was written but did not verify: " + error;
            return false;
        }
        if (!verified.records.empty() || !verified.importedProducts.empty() ||
            !verified.missingAssets.empty())
        {
            error = "New project asset registry did not verify as empty.";
            return false;
        }

        if (written.recoveryRequired)
        {
            warning = "Asset registry committed, but transaction cleanup remains pending: " +
                written.message;
        }
        error.clear();
        return true;
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
            document.defaultHud = metadata.defaultHud;
            if (!metadata.defaultHudId.empty())
            {
                ScreenDocument hud;
                std::string resolvedPath, repairedHint;
                if (!ResolveProjectScreenLibraryDocument(metadata.rootPath,
                        metadata.projectId, metadata.defaultHudId, metadata.defaultHud,
                        hud, resolvedPath, repairedHint, error))
                    return false;
                if (hud.usage != ScreenUsage::Hud || !hud.enabledInGame)
                {
                    error = "Project dependency default HUD is not an enabled HUD Screen.";
                    return false;
                }
                document.defaultHud = std::move(repairedHint);
            }
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
            // The versioned optional standard project budget document is an
            // explicit Data dependency, loaded once by Runtime, never scanned per frame.
            std::error_code budgetPathError;
            const bool budgetExists=fs::exists(fs::u8path(metadata.rootPath)/fs::u8path(ScreenPerformanceBudgetPath),budgetPathError);
            if(budgetPathError){error="Could not inspect project UI budget dependency.";return false;}
            if(budgetExists)
            {
                ScreenPerformanceBudget budget;
                if(!ReadScreenPerformanceBudget(metadata.rootPath,metadata.projectId,budget,error))return false;
                DependencyCandidate candidate;candidate.declaredPath=ScreenPerformanceBudgetPath;
                candidate.dependencyClass=DependencyClass::Data;candidate.requirement=DependencyRequirement::Required;
                candidate.provenance="Project UI performance budgets";
                document.alwaysInclude.push_back(std::move(candidate));
            }
            if(!metadata.attributeDocumentId.empty())
            {
                std::string resolved;GameplayAttributeDocument attributes;
                if(!ResolveGameplayAttributeDocumentPath(metadata.rootPath,metadata.projectId,metadata.attributeDocumentId,
                    metadata.attributeDocument,resolved,error)||!ReadGameplayAttributeDocument(resolved,metadata.projectId,attributes,error))return false;
                DependencyCandidate candidate;candidate.declaredPath=fs::relative(fs::u8path(resolved),fs::u8path(metadata.rootPath)).generic_u8string();
                candidate.dependencyClass=DependencyClass::Data;candidate.requirement=DependencyRequirement::Required;
                candidate.provenance="Project gameplay attribute module";document.alwaysInclude.push_back(std::move(candidate));
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

            std::string registryWarning;
            std::string registryError;
            if (!EnsureProjectAssetRegistry(
                    root, metadata.projectId, registryWarning, registryError))
            {
                lastError_ = std::move(registryError);
                return false;
            }
            AppendWarning(lastWarning_, std::move(registryWarning));

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

    bool ProjectService::CreateStoryFlowProject(
        const std::string& parentDirectory,
        const std::string& projectName,
        bool seedUiScaffold)
    {
        lastError_.clear();
        lastWarning_.clear();

        if (!IsValidProjectName(projectName))
        {
            lastError_ =
                "Project names cannot be empty or contain unsafe filename or project-file characters.";
            return false;
        }
        if (parentDirectory.empty() ||
            !fs::is_directory(fs::u8path(parentDirectory)))
        {
            lastError_ = "Choose an existing parent folder for the project.";
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

            if (seedUiScaffold)
            {
                // Stage on the destination volume, then publish with one rename.
                // The existing active project and recents stay untouched on failure.
                const auto container = root.parent_path() /
                    fs::u8path(".rg-create-" + GenerateStableId().substr(0,8));
                if(!fs::create_directory(container))
                {lastError_="Could not reserve a private project staging folder.";return false;}
                struct Cleanup {
                    fs::path path;
                    ~Cleanup(){std::error_code ignored;fs::remove_all(path,ignored);}
                } cleanup{container};
                ProjectService staged;
                if (!staged.CreateStoryFlowProject(container.generic_u8string(),projectName,false))
                {lastError_=staged.LastError();return false;}
                std::string scaffoldError;
                if (!SeedProjectUiScaffold(staged,scaffoldError) ||
                    !staged.OpenProject(staged.CurrentProject().descriptorPath))
                {lastError_=scaffoldError.empty()?staged.LastError():scaffoldError;return false;}
                auto metadata=staged.CurrentProject();
                const auto stagedRoot=fs::u8path(metadata.rootPath);
                // Never overwrite a folder that acquired content while seeding.
                if(fs::exists(root))
                {
                    if(!fs::is_directory(root)||!fs::is_empty(root)||!fs::remove(root))
                    {lastError_="The new project destination changed during creation.";return false;}
                }
                fs::rename(stagedRoot,root);
                metadata.rootPath=root.generic_u8string();
                metadata.descriptorPath=(root/fs::u8path(projectName+".renegade")).generic_u8string();
                currentProject_=std::move(metadata);hasProject_=true;
                lastWarning_=staged.LastWarning();AddRecent(currentProject_);return true;
            }

            std::string directoryError;
            if (!EnsureDefaultProjectDirectories(root, directoryError))
            {
                lastError_ = std::move(directoryError);
                return false;
            }

            ProjectMetadata metadata;
            metadata.formatVersion = CurrentFormatVersion;
            metadata.projectId = GenerateStableId();
            metadata.name = projectName;
            metadata.rootPath = root.generic_u8string();
            metadata.descriptorPath =
                (root / fs::u8path(projectName + ".renegade"))
                    .generic_u8string();
            metadata.startupScene.clear();
            metadata.startupFlow = DefaultStartupFlow;

            FlowDocument flow;
            flow.envelope = CreateDocumentEnvelope(
                metadata.projectId,
                StoryFlowDocumentType,
                DefaultStartupFlow,
                "renegade-story-flow-project-home-v1");
            metadata.startupFlowId = flow.envelope.documentId;

            FlowNode start;
            start.id = GenerateStableId();
            start.kind = FlowNodeKind::GameStart;
            start.name = "Game Start";
            flow.startNodeId = start.id;
            flow.nodes.push_back(std::move(start));

            std::string flowError;
            const fs::path flowPath = root / fs::u8path(DefaultStartupFlow);
            if (!WriteFlowDocument(
                    flowPath.generic_u8string(), flow, flowError))
            {
                lastError_ = "Could not create the project Story Flow: " +
                    flowError;
                return false;
            }

            if (!WriteProject(metadata))
            {
                return false;
            }

            std::string registryWarning;
            std::string registryError;
            if (!EnsureProjectAssetRegistry(
                    root, metadata.projectId, registryWarning, registryError))
            {
                lastError_ = std::move(registryError);
                return false;
            }
            AppendWarning(lastWarning_, std::move(registryWarning));

            currentProject_ = metadata;
            hasProject_ = true;
            AddRecent(metadata);
            return true;
        }
        catch (const std::exception& exception)
        {
            lastError_ =
                std::string("Could not create the Story Flow project: ") +
                exception.what();
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

        if (metadata.projectId.empty())
        {
            metadata.projectId = GenerateStableId();
            if (!WriteProject(metadata))
            {
                return false;
            }
        }

        std::string registryWarning;
        if (!EnsureProjectAssetRegistry(
                fs::u8path(metadata.rootPath), metadata.projectId,
                registryWarning, error))
        {
            lastError_ = std::move(error);
            return false;
        }
        AppendWarning(lastWarning_, std::move(registryWarning));

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

        if (currentProject_.startupScene.empty())
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

    bool ProjectService::SetDefaultHud(const std::string& documentId,const std::string& pathHint)
    {
        lastError_.clear();lastWarning_.clear();
        if(!hasProject_){lastError_="Choose a project before setting its default HUD.";return false;}
        ProjectMetadata disk;
        if(!ReadProject(currentProject_.descriptorPath,disk,lastError_,false))return false;
        if(!ProjectMetadataMatches(currentProject_,disk))
        {lastError_="Project descriptor changed externally. Reopen it before editing the HUD default.";return false;}
        if(documentId.empty()!=pathHint.empty())
        {lastError_="Choose a complete HUD reference or clear both fields.";return false;}
        std::string resolvedHint=pathHint;
        if(!documentId.empty())
        {
            if(!ValidateHudReference({documentId,pathHint},lastError_))return false;
            std::string path;ScreenDocument screen;
            if(!ResolveProjectScreenLibraryDocument(currentProject_.rootPath,currentProject_.projectId,
                    documentId,pathHint,screen,path,resolvedHint,lastError_))return false;
            if(screen.usage!=ScreenUsage::Hud || !screen.enabledInGame)
            {lastError_="The project default must be an enabled HUD Screen.";return false;}
        }
        auto updated=currentProject_;updated.defaultHudId=documentId;updated.defaultHud=resolvedHint;
        if(!WriteProject(updated))return false;
        currentProject_=std::move(updated);return true;
    }

    bool ProjectService::SetGameplayAttributes(const std::string& documentId,const std::string& pathHint)
    {
        lastError_.clear();lastWarning_.clear();
        if(!hasProject_){lastError_="No project is active.";return false;}
        if(documentId.empty()!=pathHint.empty()){lastError_="Gameplay attribute reference is incomplete.";return false;}
        std::string repaired;
        if(!documentId.empty())
        {
            if(!ValidateAttributeReference(documentId,pathHint,lastError_))return false;
            std::string resolved;GameplayAttributeDocument document;
            if(!ResolveGameplayAttributeDocumentPath(currentProject_.rootPath,currentProject_.projectId,
                documentId,pathHint,resolved,lastError_)||
                !ReadGameplayAttributeDocument(resolved,currentProject_.projectId,document,lastError_))return false;
            repaired=fs::relative(fs::u8path(resolved),fs::u8path(currentProject_.rootPath)).generic_u8string();
        }
        ProjectMetadata candidate;
        if(!ReadProject(currentProject_.descriptorPath,candidate,lastError_,false))return false;
        if(candidate.projectId!=currentProject_.projectId||
            candidate.attributeDocumentId!=currentProject_.attributeDocumentId)
        {lastError_="Project attribute-module reference changed externally. Reload the project before attaching.";return false;}
        candidate.attributeDocumentId=documentId;candidate.attributeDocument=repaired;
        const auto descriptor=fs::u8path(currentProject_.descriptorPath),root=fs::u8path(currentProject_.rootPath);
        std::vector<std::uint8_t> previous,requested;
        if(!ReadFileBytes(descriptor,previous,lastError_)||
            !PatchProjectAttributeReference(previous,documentId,repaired,requested,lastError_))return false;
        if(previous==requested){currentProject_=std::move(candidate);return true;}
        ProjectDocumentWrite backup;backup.destinationPath=ProjectDescriptorBackupPath(descriptor).generic_u8string();
        backup.content=previous;backup.validator=[previous](const std::string& path,std::string& error){return FileMatchesBytes(path,previous,error);};
        ProjectDocumentWrite write;write.destinationPath=descriptor.generic_u8string();write.content=std::move(requested);
        write.validator=[this,candidate](const std::string& path,std::string& error){
            ProjectMetadata check;
            if(!ReadProject(path,check,error,false))return false;
            if(!ProjectMetadataMatches(candidate,check)){error="Staged profile reference did not round-trip.";return false;}
            return true;
        };
        ProjectDocumentTransactionOptions options;options.allowedRoot=root.generic_u8string();
        options.journalDirectory=ProjectTransactionDirectory(root).generic_u8string();
        const auto result=ProjectDocumentTransaction{}.Execute({std::move(backup),std::move(write)},options);
        if(!result.success){lastError_="Gameplay module reference transaction failed: "+result.message;return false;}
        currentProject_=std::move(candidate);return true;
    }

    bool ProjectService::SetAlwaysInclude(
        const std::vector<std::string>& declarations)
    {
        lastError_.clear();
        lastWarning_.clear();
        if (!hasProject_)
        {
            lastError_ =
                "Could not update Always Include without an active project.";
            return false;
        }

        ProjectMetadata updated = currentProject_;
        updated.alwaysInclude = declarations;
        if (!WriteProject(updated))
            return false;

        currentProject_ = std::move(updated);
        return true;
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
            const auto attributeDocumentId=project.GetText("attribute_document_id");
            const auto attributeDocument=project.GetText("attribute_document");
            if(attributeDocumentId.empty()!=attributeDocument.empty()||
                (!attributeDocumentId.empty()&&!ValidateAttributeReference(attributeDocumentId,attributeDocument,error)))
            {if(error.empty())error="Gameplay attribute reference is incomplete.";return false;}
            const auto defaultHudId=project.GetText("default_hud_id");
            const auto defaultHud=project.GetText("default_hud");
            if((defaultHudId.empty()!=defaultHud.empty()) ||
                (!defaultHudId.empty() && !ValidateHudReference({defaultHudId,defaultHud},error)))
            {if(error.empty())error="Project default HUD reference is incomplete.";return false;}
            const bool hasStartupFlowId = !startupFlowId.empty();
            const bool hasStartupFlowHint = !startupFlow.empty();
            const bool hasStartupScreenId = !startupScreenId.empty();
            const bool hasStartupScreenHint = !startupScreen.empty();
            const bool hasStartupScene = !startupScene.empty();
            if (!IsValidProjectName(name) ||
                (hasStartupScene && !IsSafeRelativePath(startupScene)) ||
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
            if (!hasStartupScene && !hasStartupFlowId)
            {
                error = "The project descriptor does not declare a startup Scene or Story Flow.";
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
            const fs::path startupPath =
                hasStartupScene ? (root / startupScene).lexically_normal() : fs::path{};
            if (requireStartupScene && hasStartupScene &&
                !fs::is_regular_file(startupPath))
            {
                error =
                    "Project startup scene is missing: " +
                    startupPath.generic_u8string();
                return false;
            }
            if (requireStartupScene && !hasStartupScene)
            {
                std::string resolvedFlow;
                FlowDocument flow;
                if (!ResolveStoryFlowDocumentPath(
                        root.generic_u8string(), projectId,
                        startupFlowId, startupFlow.generic_u8string(),
                        resolvedFlow, error) ||
                    !ReadFlowDocument(
                        resolvedFlow, projectId, flow, error))
                {
                    error = "Project startup Story Flow is invalid: " + error;
                    return false;
                }
            }
            metadata.formatVersion = static_cast<std::uint32_t>(version);
            metadata.projectId = projectId;
            metadata.name = name;
            metadata.descriptorPath = descriptor.generic_u8string();
            metadata.rootPath = root.generic_u8string();
            metadata.startupScene = startupScene.generic_u8string();
            metadata.startupFlowId = startupFlowId;
            metadata.startupFlow = startupFlow.generic_u8string();
            metadata.attributeDocumentId=attributeDocumentId;metadata.attributeDocument=attributeDocument;
            metadata.defaultHudId=defaultHudId;
            metadata.defaultHud=defaultHud;
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
        const bool hasStartupScene = !metadata.startupScene.empty();
        if (!IsValidProjectName(metadata.name) ||
            (hasStartupScene &&
                !IsSafeRelativePath(fs::u8path(metadata.startupScene))))
        {
            lastError_ = "Could not write invalid project metadata.";
            return false;
        }

        if(metadata.attributeDocumentId.empty()!=metadata.attributeDocument.empty()||
            (!metadata.attributeDocumentId.empty()&&!ValidateAttributeReference(metadata.attributeDocumentId,metadata.attributeDocument,lastError_)))
        {if(lastError_.empty())lastError_="Gameplay attribute reference is incomplete.";return false;}
        std::string hudError;
        if((metadata.defaultHudId.empty()!=metadata.defaultHud.empty()) ||
            (!metadata.defaultHudId.empty() && !ValidateHudReference({metadata.defaultHudId,metadata.defaultHud},hudError)))
        {lastError_=hudError.empty()?"Project default HUD reference is incomplete.":hudError;return false;}
        const bool hasStartupFlowId = !metadata.startupFlowId.empty();
        const bool hasStartupFlowHint = !metadata.startupFlow.empty();
        const bool hasStartupScreenId = !metadata.startupScreenId.empty();
        const bool hasStartupScreenHint = !metadata.startupScreen.empty();
        if ((!hasStartupScene && !hasStartupFlowId) ||
            hasStartupFlowId != hasStartupFlowHint ||
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
        }
    }
}
