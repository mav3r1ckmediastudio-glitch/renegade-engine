#include "renegade/bridge/ScriptLibraryService.h"

#include "renegade/bridge/IdentityService.h"
#include "renegade/bridge/ProjectDocumentTransaction.h"

#include "json.hpp"

#include <WickedEngine.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>
#include <set>
#include <sstream>
#include <system_error>
#include <unordered_map>
#include <utility>

namespace
{
    namespace fs = std::filesystem;
    using namespace renegade::bridge;

    constexpr const char* LibraryLockSchema = "renegade-script-library-lock";
    constexpr std::uint32_t LibraryLockSchemaVersion = 1;
    constexpr std::size_t MaximumPackageFiles = 2048;
    constexpr std::size_t MaximumManifestBytes = 1024u * 1024u;
    constexpr const char* S7StockPackageId = "renegade.stock.actions.wave_a";

    bool IsRecoverableStockProjectFile(
        const std::string& packageId,
        const std::string& packageVersion,
        const std::string& filePath,
        const std::string& projectHash,
        const std::string& installedHash) noexcept
    {
        if (packageId != S7StockPackageId)
            return false;

        // Current byte-identical stock content is always safe to reclaim.
        if (projectHash == installedHash)
            return true;

        // S7 v1.0 shipped before the package-ownership repair. Those builds
        // could strand an official Action on disk without its lock record.
        // v1.1 is allowed to migrate only the exact known official v1.0 bytes;
        // arbitrary or creator-edited files remain collisions.
        if (packageVersion != "1.1.0")
            return false;

        return
            (filePath == "Door.lua" &&
                projectHash == "fnv1a64:13d97d4492f8089e") ||
            (filePath == "Switch.lua" &&
                projectHash == "fnv1a64:e0aa38cd6391622c") ||
            (filePath == "Pickup.lua" &&
                projectHash == "fnv1a64:9fa1dd43736335c5");
    }

    struct PackageFile
    {
        std::string path;
        std::string contentHash;
        std::vector<std::string> dependencies;
    };

    struct PackageDocument
    {
        std::string manifestPath;
        std::string rootPath;
        std::string packageId;
        std::string packageVersion;
        std::string displayName;
        std::vector<PackageFile> files;
        std::vector<std::string> entries;
    };

    struct LockRecord
    {
        std::string packageId;
        std::string packageVersion;
        std::string sourcePath;
        std::string projectPath;
        StableId sourceId;
        std::string contentHash;
    };

    struct LockDocument
    {
        std::vector<LockRecord> records;
    };

    std::string PathKey(const std::string& path)
    {
        std::string value = fs::u8path(path).lexically_normal().generic_u8string();
        std::transform(
            value.begin(), value.end(), value.begin(),
            [](const unsigned char c)
            {
                return static_cast<char>(std::tolower(c));
            });
        return value;
    }

    bool IsSafePackageId(const std::string& value)
    {
        if (value.empty() || value.size() > 96)
            return false;
        return std::all_of(
            value.begin(), value.end(),
            [](const unsigned char c)
            {
                return std::isalnum(c) != 0 || c == '.' || c == '_' || c == '-';
            });
    }

    bool IsSafeRelativePath(const std::string& value)
    {
        if (value.empty())
            return false;
        const fs::path path = fs::u8path(value);
        if (path.is_absolute() || path.has_root_name() || path.has_root_directory())
            return false;
        for (const auto& part : path)
        {
            if (part.empty() || part == "." || part == "..")
                return false;
        }
        return true;
    }

    std::uint64_t Fnv1a64(
        const std::uint8_t* data,
        const std::size_t size,
        std::uint64_t value = 14695981039346656037ull)
    {
        for (std::size_t index = 0; index < size; ++index)
        {
            value ^= static_cast<std::uint64_t>(data[index]);
            value *= 1099511628211ull;
        }
        return value;
    }

    std::uint64_t Fnv1a64(const std::string& value)
    {
        return Fnv1a64(
            reinterpret_cast<const std::uint8_t*>(value.data()),
            value.size());
    }

    std::string Hex64(const std::uint64_t value)
    {
        std::ostringstream stream;
        stream << std::hex << std::setfill('0') << std::setw(16) << value;
        return stream.str();
    }

    std::string HashBytes(const std::vector<std::uint8_t>& bytes)
    {
        return "fnv1a64:" + Hex64(Fnv1a64(bytes.data(), bytes.size()));
    }

    bool ReadBytes(
        const fs::path& path,
        std::vector<std::uint8_t>& bytes,
        std::string& error,
        const std::size_t maximumBytes = MaximumScriptSourceBytes)
    {
        bytes.clear();
        std::error_code ec;
        const auto size = fs::file_size(path, ec);
        if (ec)
        {
            error = "Could not inspect script package file: " + ec.message();
            return false;
        }
        if (size > maximumBytes)
        {
            error = "Script package file exceeds the governed size limit: " +
                path.generic_u8string();
            return false;
        }
        std::ifstream stream(path, std::ios::binary);
        if (!stream)
        {
            error = "Could not open script package file: " + path.generic_u8string();
            return false;
        }
        bytes.assign(
            std::istreambuf_iterator<char>(stream),
            std::istreambuf_iterator<char>());
        if (stream.bad())
        {
            bytes.clear();
            error = "Could not read complete script package file: " +
                path.generic_u8string();
            return false;
        }
        error.clear();
        return true;
    }

    bool HashFile(
        const fs::path& path,
        std::string& hash,
        std::string& error)
    {
        std::vector<std::uint8_t> bytes;
        if (!ReadBytes(path, bytes, error))
            return false;
        hash = HashBytes(bytes);
        error.clear();
        return true;
    }

    bool ResolveContainedFile(
        const fs::path& root,
        const std::string& relativePath,
        fs::path& absolutePath,
        std::string& error)
    {
        if (!IsSafeRelativePath(relativePath))
        {
            error = "Script package contains an unsafe relative path: " + relativePath;
            return false;
        }

        std::error_code ec;
        const fs::path canonicalRoot = fs::weakly_canonical(root, ec);
        if (ec || canonicalRoot.empty())
        {
            error = "Could not resolve script package root: " + ec.message();
            return false;
        }
        const fs::path candidate = fs::weakly_canonical(
            canonicalRoot / fs::u8path(relativePath), ec);
        if (ec || candidate.empty())
        {
            error = "Could not resolve script package file '" + relativePath + "'.";
            return false;
        }
        const fs::path relative = fs::relative(candidate, canonicalRoot, ec);
        if (ec || relative.empty() || relative.is_absolute())
        {
            error = "Script package file escapes its package root: " + relativePath;
            return false;
        }
        for (const auto& part : relative)
        {
            if (part == "..")
            {
                error = "Script package file escapes its package root: " + relativePath;
                return false;
            }
        }
        if (!fs::is_regular_file(candidate, ec) || ec)
        {
            error = "Script package file is missing: " + relativePath;
            return false;
        }
        absolutePath = candidate;
        error.clear();
        return true;
    }

    StableId PackageFileStableId(
        const std::string& packageId,
        const std::string& relativePath)
    {
        const std::string key = packageId + "\n" + PathKey(relativePath);
        std::string raw = Hex64(Fnv1a64(key)) + Hex64(Fnv1a64(key + "\nrenegade-s6"));
        raw[12] = '4';
        raw[16] = '8';
        return raw.substr(0, 8) + "-" + raw.substr(8, 4) + "-" +
            raw.substr(12, 4) + "-" + raw.substr(16, 4) + "-" +
            raw.substr(20, 12);
    }

    std::string ProjectPathFor(
        const std::string& packageId,
        const std::string& packageRelativePath)
    {
        return (
            fs::path("Content") / "Scripts" / "Library" /
            fs::u8path(packageId) / fs::u8path(packageRelativePath))
            .lexically_normal()
            .generic_u8string();
    }

    ScriptMetadataDiagnostic LibraryDiagnostic(
        std::string code,
        std::string sourcePath,
        std::string message)
    {
        ScriptMetadataDiagnostic diagnostic;
        diagnostic.severity = ScriptMetadataDiagnosticSeverity::Error;
        diagnostic.code = std::move(code);
        diagnostic.sourcePath = std::move(sourcePath);
        diagnostic.field = "library";
        diagnostic.message = std::move(message);
        return diagnostic;
    }

    ScriptMetadataEvaluationResult EvaluatePackageMetadata(
        const std::string& packageRoot,
        const std::string& packageRelativePath)
    {
        ScriptMetadataEvaluationResult result;
        fs::path source;
        std::string error;
        if (!ResolveContainedFile(
                fs::u8path(packageRoot),
                packageRelativePath,
                source,
                error))
        {
            result.diagnostics.push_back(LibraryDiagnostic(
                "metadata.source_invalid",
                packageRelativePath,
                error));
            return result;
        }

        std::error_code ec;
        const fs::path systemTemp = fs::temp_directory_path(ec);
        if (ec || systemTemp.empty())
        {
            result.diagnostics.push_back(LibraryDiagnostic(
                "metadata.preview_unavailable",
                packageRelativePath,
                "Could not resolve a temporary metadata sandbox."));
            return result;
        }

        const fs::path previewRoot =
            systemTemp / fs::u8path("renegade-s6-metadata-" + GenerateStableId());
        const fs::path previewSource =
            previewRoot / "Content" / "Scripts" / "LibraryPreview.lua";
        fs::create_directories(previewSource.parent_path(), ec);
        if (!ec)
        {
            fs::copy_file(
                source,
                previewSource,
                fs::copy_options::overwrite_existing,
                ec);
        }
        if (ec)
        {
            std::error_code cleanupError;
            fs::remove_all(previewRoot, cleanupError);
            result.diagnostics.push_back(LibraryDiagnostic(
                "metadata.preview_unavailable",
                packageRelativePath,
                "Could not prepare the restricted package metadata sandbox: " +
                    ec.message()));
            return result;
        }

        result = EvaluateScriptMetadata(
            previewRoot.generic_u8string(),
            "Content/Scripts/LibraryPreview.lua");
        for (auto& diagnostic : result.diagnostics)
            diagnostic.sourcePath = packageRelativePath;

        std::error_code cleanupError;
        fs::remove_all(previewRoot, cleanupError);
        return result;
    }

    bool ReadPackage(
        const fs::path& manifestPath,
        PackageDocument& package,
        std::string& error)
    {
        package = {};
        std::vector<std::uint8_t> bytes;
        if (!ReadBytes(manifestPath, bytes, error, MaximumManifestBytes))
            return false;

        const std::string text(bytes.begin(), bytes.end());
        const nlohmann::json document = nlohmann::json::parse(
            text, nullptr, false);
        if (document.is_discarded() || !document.is_object())
        {
            error = "Script package manifest is not valid JSON.";
            return false;
        }
        if (document.value("schema", std::string{}) != ScriptLibraryPackageSchema ||
            document.value("schema_version", 0u) != ScriptLibraryPackageSchemaVersion)
        {
            error = "Script package manifest has an unsupported schema.";
            return false;
        }

        package.manifestPath = manifestPath.lexically_normal().generic_u8string();
        package.rootPath = manifestPath.parent_path().lexically_normal().generic_u8string();
        package.packageId = document.value("package_id", std::string{});
        package.packageVersion = document.value("package_version", std::string{});
        package.displayName = document.value("display_name", package.packageId);
        if (!IsSafePackageId(package.packageId) || package.packageVersion.empty() ||
            package.displayName.empty())
        {
            error = "Script package manifest has invalid package identity metadata.";
            return false;
        }
        if (!document.contains("files") || !document.at("files").is_array() ||
            document.at("files").empty() ||
            document.at("files").size() > MaximumPackageFiles ||
            !document.contains("entries") || !document.at("entries").is_array() ||
            document.at("entries").empty())
        {
            error = "Script package manifest requires non-empty files and entries arrays.";
            return false;
        }

        std::set<std::string> pathKeys;
        for (const auto& item : document.at("files"))
        {
            if (!item.is_object())
            {
                error = "Script package files must be objects.";
                return false;
            }
            PackageFile file;
            file.path = item.value("path", std::string{});
            file.contentHash = item.value("content_hash", std::string{});
            if (!IsSafeRelativePath(file.path) ||
                fs::u8path(file.path).extension() != ".lua" ||
                file.contentHash.rfind("fnv1a64:", 0) != 0 ||
                file.contentHash.size() != 24)
            {
                error = "Script package contains an invalid Lua file declaration.";
                return false;
            }
            if (!pathKeys.insert(PathKey(file.path)).second)
            {
                error = "Script package contains duplicate or case-colliding file paths.";
                return false;
            }
            if (item.contains("dependencies"))
            {
                if (!item.at("dependencies").is_array())
                {
                    error = "Script package file dependencies must be an array.";
                    return false;
                }
                for (const auto& dependency : item.at("dependencies"))
                {
                    if (!dependency.is_string() ||
                        !IsSafeRelativePath(dependency.get<std::string>()))
                    {
                        error = "Script package contains an invalid dependency path.";
                        return false;
                    }
                    file.dependencies.push_back(dependency.get<std::string>());
                }
                std::sort(file.dependencies.begin(), file.dependencies.end());
                file.dependencies.erase(
                    std::unique(file.dependencies.begin(), file.dependencies.end()),
                    file.dependencies.end());
            }
            package.files.push_back(std::move(file));
        }

        std::map<std::string, const PackageFile*> filesByPath;
        for (const auto& file : package.files)
            filesByPath.emplace(file.path, &file);
        for (const auto& file : package.files)
        {
            for (const auto& dependency : file.dependencies)
            {
                if (filesByPath.find(dependency) == filesByPath.end())
                {
                    error = "Script package dependency is not declared in files: " +
                        dependency;
                    return false;
                }
            }
        }

        std::set<std::string> entryKeys;
        for (const auto& item : document.at("entries"))
        {
            if (!item.is_string())
            {
                error = "Script package entries must be file paths.";
                return false;
            }
            const std::string entry = item.get<std::string>();
            if (filesByPath.find(entry) == filesByPath.end() ||
                !entryKeys.insert(PathKey(entry)).second)
            {
                error = "Script package entry is missing, duplicated or case-colliding: " +
                    entry;
                return false;
            }
            package.entries.push_back(entry);
        }
        std::sort(package.entries.begin(), package.entries.end());

        for (const auto& file : package.files)
        {
            fs::path source;
            if (!ResolveContainedFile(
                    fs::u8path(package.rootPath), file.path, source, error))
            {
                return false;
            }
            std::string actualHash;
            if (!HashFile(source, actualHash, error))
                return false;
            if (actualHash != file.contentHash)
            {
                error = "Script package immutable hash mismatch for '" +
                    file.path + "'. Expected " + file.contentHash +
                    ", found " + actualHash + ".";
                return false;
            }
        }

        error.clear();
        return true;
    }

    const PackageFile* FindPackageFile(
        const PackageDocument& package,
        const std::string& path)
    {
        const auto found = std::find_if(
            package.files.begin(), package.files.end(),
            [&path](const PackageFile& file)
            {
                return file.path == path;
            });
        return found == package.files.end() ? nullptr : &*found;
    }

    bool BuildClosure(
        const PackageDocument& package,
        const std::string& entryPath,
        std::vector<const PackageFile*>& closure,
        std::string& error)
    {
        closure.clear();
        if (std::find(package.entries.begin(), package.entries.end(), entryPath) ==
            package.entries.end())
        {
            error = "Requested script package entry is not declared: " + entryPath;
            return false;
        }

        std::map<std::string, int> state;
        const auto visit = [&](const auto& self, const std::string& path) -> bool
        {
            const PackageFile* file = FindPackageFile(package, path);
            if (file == nullptr)
            {
                error = "Script package closure references an unknown file: " + path;
                return false;
            }
            const int current = state[path];
            if (current == 2)
                return true;
            if (current == 1)
            {
                error = "Script package dependency cycle detected at: " + path;
                return false;
            }
            state[path] = 1;
            for (const auto& dependency : file->dependencies)
            {
                if (!self(self, dependency))
                    return false;
            }
            state[path] = 2;
            closure.push_back(file);
            return true;
        };
        if (!visit(visit, entryPath))
            return false;

        std::sort(
            closure.begin(), closure.end(),
            [](const PackageFile* left, const PackageFile* right)
            {
                return left->path < right->path;
            });
        error.clear();
        return true;
    }

    fs::path LockPath(const std::string& projectRoot)
    {
        return fs::u8path(projectRoot) / fs::u8path(ScriptLibraryLockRelativePath);
    }

    bool ReadLock(
        const std::string& projectRoot,
        LockDocument& lock,
        std::string& error)
    {
        lock = {};
        const fs::path path = LockPath(projectRoot);
        std::error_code ec;
        if (!fs::exists(path, ec))
        {
            if (ec)
            {
                error = "Could not inspect S6 script library lock: " + ec.message();
                return false;
            }
            error.clear();
            return true;
        }
        if (!fs::is_regular_file(path, ec) || ec)
        {
            error = "S6 script library lock is not a regular file.";
            return false;
        }

        std::vector<std::uint8_t> bytes;
        if (!ReadBytes(path, bytes, error, MaximumManifestBytes))
            return false;
        const std::string text(bytes.begin(), bytes.end());
        const nlohmann::json document = nlohmann::json::parse(
            text, nullptr, false);
        if (document.is_discarded() || !document.is_object() ||
            document.value("schema", std::string{}) != LibraryLockSchema ||
            document.value("schema_version", 0u) != LibraryLockSchemaVersion ||
            !document.contains("records") || !document.at("records").is_array())
        {
            error = "S6 script library lock has an invalid schema.";
            return false;
        }

        std::set<std::string> uniquePaths;
        for (const auto& item : document.at("records"))
        {
            if (!item.is_object())
            {
                error = "S6 script library lock record is invalid.";
                return false;
            }
            LockRecord record;
            record.packageId = item.value("package_id", std::string{});
            record.packageVersion = item.value("package_version", std::string{});
            record.sourcePath = item.value("source_path", std::string{});
            record.projectPath = item.value("project_path", std::string{});
            record.sourceId = item.value("source_id", std::string{});
            record.contentHash = item.value("content_hash", std::string{});
            if (!IsSafePackageId(record.packageId) ||
                record.packageVersion.empty() ||
                !IsSafeRelativePath(record.sourcePath) ||
                !IsSafeRelativePath(record.projectPath) ||
                !IsValidStableId(record.sourceId) ||
                record.contentHash.rfind("fnv1a64:", 0) != 0 ||
                !uniquePaths.insert(PathKey(record.projectPath)).second)
            {
                error = "S6 script library lock contains an invalid record.";
                return false;
            }
            lock.records.push_back(std::move(record));
        }
        error.clear();
        return true;
    }

    std::vector<std::uint8_t> SerializeLock(const LockDocument& lock)
    {
        auto records = lock.records;
        std::sort(
            records.begin(), records.end(),
            [](const LockRecord& left, const LockRecord& right)
            {
                if (left.packageId != right.packageId)
                    return left.packageId < right.packageId;
                return left.projectPath < right.projectPath;
            });

        nlohmann::json document;
        document["schema"] = LibraryLockSchema;
        document["schema_version"] = LibraryLockSchemaVersion;
        document["records"] = nlohmann::json::array();
        for (const auto& record : records)
        {
            document["records"].push_back({
                {"package_id", record.packageId},
                {"package_version", record.packageVersion},
                {"source_path", record.sourcePath},
                {"project_path", record.projectPath},
                {"source_id", record.sourceId},
                {"content_hash", record.contentHash},
            });
        }
        const std::string text = document.dump(2) + "\n";
        return std::vector<std::uint8_t>(text.begin(), text.end());
    }

    const LockRecord* FindLockRecord(
        const LockDocument& lock,
        const std::string& packageId,
        const std::string& projectPath)
    {
        const std::string key = PathKey(projectPath);
        const auto found = std::find_if(
            lock.records.begin(), lock.records.end(),
            [&](const LockRecord& record)
            {
                return record.packageId == packageId &&
                    PathKey(record.projectPath) == key;
            });
        return found == lock.records.end() ? nullptr : &*found;
    }

    std::string LockedPackageVersion(
        const LockDocument& lock,
        const std::string& packageId)
    {
        const auto found = std::find_if(
            lock.records.begin(), lock.records.end(),
            [&](const LockRecord& record)
            {
                return record.packageId == packageId;
            });
        return found == lock.records.end() ? std::string{} : found->packageVersion;
    }

    bool CurrentFileHash(
        const std::string& projectRoot,
        const std::string& projectPath,
        bool& exists,
        std::string& hash,
        std::string& error)
    {
        const fs::path path = fs::u8path(projectRoot) / fs::u8path(projectPath);
        std::error_code ec;
        exists = fs::exists(path, ec);
        if (ec)
        {
            error = "Could not inspect adopted script file: " + ec.message();
            return false;
        }
        if (!exists)
        {
            hash.clear();
            error.clear();
            return true;
        }
        if (!fs::is_regular_file(path, ec) || ec)
        {
            error = "Adopted script path is not a regular file: " + projectPath;
            return false;
        }
        return HashFile(path, hash, error);
    }

    bool PackageHasLocalConflict(
        const std::string& projectRoot,
        const LockDocument& lock,
        const std::string& packageId,
        bool& conflict,
        std::string& error)
    {
        conflict = false;
        for (const auto& record : lock.records)
        {
            if (record.packageId != packageId)
                continue;
            bool exists = false;
            std::string hash;
            if (!CurrentFileHash(
                    projectRoot, record.projectPath, exists, hash, error))
            {
                return false;
            }
            if (exists && hash != record.contentHash)
            {
                conflict = true;
                error.clear();
                return true;
            }
        }
        error.clear();
        return true;
    }

    ScriptSourceBinding BuildBinding(
        const PackageDocument& package,
        const PackageFile& entry,
        const std::vector<const PackageFile*>& closure)
    {
        ScriptSourceBinding binding;
        binding.sourceId = PackageFileStableId(package.packageId, entry.path);
        binding.sourcePath = ProjectPathFor(package.packageId, entry.path);
        binding.apiVersion = 1;
        binding.unsafe = false;
        binding.provenance.kind = ScriptProvenanceKind::InstalledLibrary;
        binding.provenance.libraryId = package.packageId;
        binding.provenance.libraryVersion = package.packageVersion;
        binding.provenance.contentHash = entry.contentHash;
        for (const PackageFile* file : closure)
        {
            if (file == nullptr || file->path == entry.path)
                continue;
            ScriptDependency dependency;
            dependency.kind = ScriptDependencyKind::ScriptModule;
            dependency.id = PackageFileStableId(package.packageId, file->path);
            dependency.pathHint = ProjectPathFor(package.packageId, file->path);
            dependency.optional = false;
            binding.dependencies.push_back(std::move(dependency));
        }
        std::sort(
            binding.dependencies.begin(), binding.dependencies.end(),
            [](const ScriptDependency& left, const ScriptDependency& right)
            {
                return left.pathHint < right.pathHint;
            });
        return binding;
    }

    bool BuildEntry(
        const std::string& projectRoot,
        const PackageDocument& package,
        const std::string& entryPath,
        const LockDocument& lock,
        ScriptLibraryEntry& entry,
        std::vector<ScriptMetadataDiagnostic>& diagnostics,
        std::string& error)
    {
        std::vector<const PackageFile*> closure;
        if (!BuildClosure(package, entryPath, closure, error))
            return false;
        const PackageFile* packageEntry = FindPackageFile(package, entryPath);
        if (packageEntry == nullptr)
        {
            error = "Script package entry disappeared during closure construction.";
            return false;
        }

        auto evaluated = EvaluatePackageMetadata(package.rootPath, entryPath);
        if (!evaluated.succeeded)
        {
            diagnostics.insert(
                diagnostics.end(),
                evaluated.diagnostics.begin(), evaluated.diagnostics.end());
            error.clear();
            return true;
        }

        entry = {};
        entry.manifestPath = package.manifestPath;
        entry.packageRoot = package.rootPath;
        entry.packageId = package.packageId;
        entry.packageVersion = package.packageVersion;
        entry.packageName = package.displayName;
        entry.entryPath = entryPath;
        entry.projectSourcePath = ProjectPathFor(package.packageId, entryPath);
        entry.metadata = std::move(evaluated.descriptor);
        const std::string originalCategory = entry.metadata.category;
        entry.metadata.category = "LIBRARY / " + package.displayName;
        if (!originalCategory.empty())
            entry.metadata.category += " / " + originalCategory;
        entry.binding = BuildBinding(package, *packageEntry, closure);
        entry.binding.presentation = entry.metadata.presentation;
        for (const PackageFile* file : closure)
            entry.projectClosurePaths.push_back(ProjectPathFor(package.packageId, file->path));

        const std::string lockedVersion = LockedPackageVersion(lock, package.packageId);
        entry.updateAvailable = !lockedVersion.empty() &&
            lockedVersion != package.packageVersion;
        if (!PackageHasLocalConflict(
                projectRoot, lock, package.packageId, entry.localConflict, error))
        {
            return false;
        }

        for (const PackageFile* file : closure)
        {
            const std::string projectPath = ProjectPathFor(package.packageId, file->path);
            const LockRecord* record = FindLockRecord(lock, package.packageId, projectPath);
            if (record != nullptr &&
                record->packageVersion == package.packageVersion &&
                record->contentHash != file->contentHash)
            {
                entry.localConflict = true;
                diagnostics.push_back(LibraryDiagnostic(
                    "S6_LIBRARY_IMMUTABLE_VERSION_CHANGED",
                    package.manifestPath,
                    "Installed package '" + package.packageId + "' version '" +
                        package.packageVersion +
                        "' changed content after adoption. Renegade will not overwrite the project copy."));
            }
        }

        const LockRecord* entryRecord = FindLockRecord(
            lock, package.packageId, entry.projectSourcePath);
        if (entryRecord != nullptr &&
            entryRecord->packageVersion == package.packageVersion &&
            entryRecord->contentHash == packageEntry->contentHash)
        {
            bool exists = false;
            std::string currentHash;
            if (!CurrentFileHash(
                    projectRoot,
                    entry.projectSourcePath,
                    exists,
                    currentHash,
                    error))
            {
                return false;
            }
            entry.adopted = exists && currentHash == entryRecord->contentHash;
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
        if (!ReadBytes(fs::u8path(path), actual, error, MaximumManifestBytes))
            return false;
        if (actual != expected)
        {
            error = "Staged S6 document did not round-trip exactly.";
            return false;
        }
        error.clear();
        return true;
    }
}

namespace renegade::bridge
{
    std::string ScriptLibraryService::DefaultInstalledLibraryRoot()
    {
        if (const char* configured = std::getenv("RENEGADE_SCRIPT_LIBRARY"))
        {
            if (*configured != '\0')
                return fs::u8path(configured).lexically_normal().generic_u8string();
        }
        std::error_code ec;
        const fs::path cwd = fs::current_path(ec);
        if (ec)
            return {};
        return (cwd / "Library" / "Scripts").lexically_normal().generic_u8string();
    }

    bool ScriptLibraryService::EnumerateEntries(
        const std::string& projectRoot,
        const ScriptPresentation presentation,
        std::vector<ScriptLibraryEntry>& entries,
        std::vector<ScriptMetadataDiagnostic>& diagnostics,
        std::string& error,
        const std::string& installedLibraryRoot) const
    {
        entries.clear();
        if (projectRoot.empty())
        {
            error = "S6 Creator Library requires an active project root.";
            return false;
        }

        LockDocument lock;
        if (!ReadLock(projectRoot, lock, error))
            return false;

        struct LibraryRootCandidate
        {
            fs::path path;
            bool builtIn = false;
        };

        std::vector<LibraryRootCandidate> roots;
        const bool defaultDiscovery = installedLibraryRoot.empty();
        if (!defaultDiscovery)
        {
            roots.push_back({ fs::u8path(installedLibraryRoot), false });
        }
        else
        {
            // Native Open/Save dialogs can change the process current working
            // directory. Bundled Creator Library content belongs to the
            // executable, so resolve that stable location first.
            const std::string executablePath = wi::helper::GetExecutablePath();
            if (!executablePath.empty())
            {
                roots.push_back({
                    (fs::u8path(executablePath).parent_path() /
                        "Content" / "ScriptLibrary").lexically_normal(),
                    true,
                });
            }

            // Retain cwd as a development/test fallback for non-native hosts.
            std::error_code cwdError;
            const fs::path cwd = fs::current_path(cwdError);
            if (!cwdError && !cwd.empty())
            {
                const fs::path cwdRoot =
                    (cwd / "Content" / "ScriptLibrary").lexically_normal();
                const std::string cwdKey = PathKey(cwdRoot.generic_u8string());
                const bool alreadyPresent = std::any_of(
                    roots.begin(), roots.end(),
                    [&](const LibraryRootCandidate& candidate)
                    {
                        return PathKey(candidate.path.generic_u8string()) == cwdKey;
                    });
                if (!alreadyPresent)
                    roots.push_back({ cwdRoot, true });
            }

            const std::string configuredRoot = DefaultInstalledLibraryRoot();
            if (!configuredRoot.empty())
            {
                const fs::path configured =
                    fs::u8path(configuredRoot).lexically_normal();
                const std::string configuredKey =
                    PathKey(configured.generic_u8string());
                const bool alreadyPresent = std::any_of(
                    roots.begin(), roots.end(),
                    [&](const LibraryRootCandidate& candidate)
                    {
                        return PathKey(candidate.path.generic_u8string()) ==
                            configuredKey;
                    });
                if (!alreadyPresent)
                    roots.push_back({ configured, false });
            }
        }

        if (roots.empty())
        {
            error.clear();
            return true;
        }

        std::set<std::string> packageIds;
        for (const LibraryRootCandidate& candidate : roots)
        {
            const fs::path& root = candidate.path;
            std::error_code ec;
            if (!fs::exists(root, ec))
            {
                if (ec)
                {
                    error = "Could not inspect installed script library: " +
                        ec.message();
                    return false;
                }
                continue;
            }
            if (!fs::is_directory(root, ec) || ec)
            {
                error = "Installed script library root is not a readable directory.";
                return false;
            }

            std::vector<fs::path> manifests;
            fs::recursive_directory_iterator iterator(
                root, fs::directory_options::skip_permission_denied, ec);
            const fs::recursive_directory_iterator end;
            if (ec)
            {
                error = "Could not enumerate installed script library: " + ec.message();
                return false;
            }
            for (; iterator != end; iterator.increment(ec))
            {
                if (ec)
                {
                    error = "Could not enumerate installed script library: " +
                        ec.message();
                    return false;
                }
                std::error_code typeError;
                if (!iterator->is_regular_file(typeError) || typeError)
                    continue;
                if (iterator->path().filename() == ScriptLibraryPackageFilename)
                    manifests.push_back(iterator->path());
            }
            std::sort(manifests.begin(), manifests.end());

            for (const fs::path& manifest : manifests)
            {
                PackageDocument package;
                std::string packageError;
                if (!ReadPackage(manifest, package, packageError))
                {
                    diagnostics.push_back(LibraryDiagnostic(
                        "S6_LIBRARY_PACKAGE_INVALID",
                        manifest.generic_u8string(),
                        packageError));
                    continue;
                }

                if (defaultDiscovery &&
                    !candidate.builtIn &&
                    package.packageId == S7StockPackageId)
                {
                    diagnostics.push_back(LibraryDiagnostic(
                        "S7_STOCK_LIBRARY_SHADOW_BLOCKED",
                        package.manifestPath,
                        "External Creator Library package attempted to shadow the built-in Renegade Stock Actions package ID. The built-in package remains authoritative."));
                    continue;
                }

                if (!packageIds.insert(package.packageId).second)
                {
                    diagnostics.push_back(LibraryDiagnostic(
                        "S6_LIBRARY_PACKAGE_ID_COLLISION",
                        package.manifestPath,
                        "Duplicate installed script package ID: " + package.packageId));
                    continue;
                }

                for (const auto& entryPath : package.entries)
                {
                    ScriptLibraryEntry entry;
                    if (!BuildEntry(
                            projectRoot,
                            package,
                            entryPath,
                            lock,
                            entry,
                            diagnostics,
                            packageError))
                    {
                        diagnostics.push_back(LibraryDiagnostic(
                            "S6_LIBRARY_CLOSURE_INVALID",
                            package.manifestPath,
                            packageError));
                        continue;
                    }
                    if (entry.projectSourcePath.empty() ||
                        entry.metadata.presentation != presentation)
                    {
                        continue;
                    }
                    entries.push_back(std::move(entry));
                }
            }
        }

        std::sort(
            entries.begin(), entries.end(),
            [](const ScriptLibraryEntry& left, const ScriptLibraryEntry& right)
            {
                if (left.packageName != right.packageName)
                    return left.packageName < right.packageName;
                if (left.metadata.category != right.metadata.category)
                    return left.metadata.category < right.metadata.category;
                if (left.metadata.name != right.metadata.name)
                    return left.metadata.name < right.metadata.name;
                return left.projectSourcePath < right.projectSourcePath;
            });
        error.clear();
        return true;
    }

    bool ScriptLibraryService::AdoptEntry(
        const std::string& projectRoot,
        const std::string& manifestPath,
        const std::string& entryPath,
        ScriptLibraryAdoptionResult& result,
        std::string& error) const
    {
        result = {};
        if (projectRoot.empty() || manifestPath.empty() || entryPath.empty())
        {
            error = "S6 adoption requires project, manifest and entry paths.";
            return false;
        }

        PackageDocument package;
        if (!ReadPackage(fs::u8path(manifestPath), package, error))
            return false;
        std::vector<const PackageFile*> closure;
        if (!BuildClosure(package, entryPath, closure, error))
            return false;
        const PackageFile* packageEntry = FindPackageFile(package, entryPath);
        if (packageEntry == nullptr)
        {
            error = "S6 adoption could not resolve its package entry.";
            return false;
        }

        LockDocument lock;
        if (!ReadLock(projectRoot, lock, error))
            return false;
        const std::string previousVersion = LockedPackageVersion(lock, package.packageId);

        bool packageConflict = false;
        if (!PackageHasLocalConflict(
                projectRoot, lock, package.packageId, packageConflict, error))
        {
            return false;
        }
        if (packageConflict)
        {
            error = "S6 update blocked: project-owned files from package '" +
                package.packageId +
                "' have local edits. Renegade will not overwrite creator changes.";
            return false;
        }

        std::set<std::string> closureProjectKeys;
        for (const PackageFile* file : closure)
        {
            closureProjectKeys.insert(PathKey(
                ProjectPathFor(package.packageId, file->path)));

            const std::string projectPath = ProjectPathFor(package.packageId, file->path);
            const LockRecord* previous = FindLockRecord(lock, package.packageId, projectPath);
            if (previous != nullptr &&
                previous->packageVersion == package.packageVersion &&
                previous->contentHash != file->contentHash)
            {
                error = "S6 immutable package violation: installed package '" +
                    package.packageId + "' version '" + package.packageVersion +
                    "' changed content after adoption.";
                return false;
            }

            bool exists = false;
            std::string currentHash;
            if (!CurrentFileHash(
                    projectRoot, projectPath, exists, currentHash, error))
            {
                return false;
            }
            if (exists && previous == nullptr)
            {
                // Early S7 builds incorrectly removed ownership records for
                // previously adopted stock Actions while leaving their exact
                // shipped Lua files behind. Reclaim only byte-identical stock
                // content. Any changed/unrelated file remains creator-owned and
                // blocks adoption exactly as before.
                if (!IsRecoverableStockProjectFile(
                        package.packageId,
                        package.packageVersion,
                        file->path,
                        currentHash,
                        file->contentHash))
                {
                    error = "S6 adoption collision: project path already exists outside library authority: " +
                        projectPath;
                    return false;
                }
            }
            if (exists && previous != nullptr && currentHash != previous->contentHash)
            {
                error = "S6 adoption blocked by creator-modified project copy: " +
                    projectPath;
                return false;
            }
        }

        LockDocument candidate = lock;
        candidate.records.erase(
            std::remove_if(
                candidate.records.begin(), candidate.records.end(),
                [&](const LockRecord& record)
                {
                    if (record.packageId != package.packageId)
                        return false;
                    if (package.packageId != S7StockPackageId)
                        return true;
                    return closureProjectKeys.count(PathKey(record.projectPath)) != 0;
                }),
            candidate.records.end());

        std::vector<ProjectDocumentWrite> writes;
        writes.reserve(closure.size() + 1);
        for (const PackageFile* file : closure)
        {
            fs::path sourcePath;
            if (!ResolveContainedFile(
                    fs::u8path(package.rootPath), file->path, sourcePath, error))
            {
                return false;
            }
            std::vector<std::uint8_t> bytes;
            if (!ReadBytes(sourcePath, bytes, error))
                return false;
            if (HashBytes(bytes) != file->contentHash)
            {
                error = "S6 package changed during adoption: " + file->path;
                return false;
            }

            const std::string projectPath = ProjectPathFor(package.packageId, file->path);
            const fs::path destination =
                fs::u8path(projectRoot) / fs::u8path(projectPath);
            std::error_code directoryError;
            fs::create_directories(destination.parent_path(), directoryError);
            if (directoryError)
            {
                error = "Could not prepare S6 adoption directory: " +
                    directoryError.message();
                return false;
            }

            ProjectDocumentWrite write;
            write.destinationPath = destination.lexically_normal().generic_u8string();
            write.content = bytes;
            write.validator = [bytes](const std::string& path, std::string& validationError)
            {
                return FileMatchesBytes(path, bytes, validationError);
            };
            writes.push_back(std::move(write));

            LockRecord record;
            record.packageId = package.packageId;
            record.packageVersion = package.packageVersion;
            record.sourcePath = file->path;
            record.projectPath = projectPath;
            record.sourceId = PackageFileStableId(package.packageId, file->path);
            record.contentHash = file->contentHash;
            candidate.records.push_back(std::move(record));
            result.projectClosurePaths.push_back(projectPath);
        }

        const std::vector<std::uint8_t> lockBytes = SerializeLock(candidate);
        const fs::path lockPath = LockPath(projectRoot);
        std::error_code directoryError;
        fs::create_directories(lockPath.parent_path(), directoryError);
        if (directoryError)
        {
            error = "Could not prepare S6 lock directory: " + directoryError.message();
            return false;
        }
        ProjectDocumentWrite lockWrite;
        lockWrite.destinationPath = lockPath.lexically_normal().generic_u8string();
        lockWrite.content = lockBytes;
        lockWrite.validator = [lockBytes](const std::string& path, std::string& validationError)
        {
            return FileMatchesBytes(path, lockBytes, validationError);
        };
        writes.push_back(std::move(lockWrite));

        ProjectDocumentTransactionOptions options;
        options.allowedRoot = fs::u8path(projectRoot).lexically_normal().generic_u8string();
        options.journalDirectory =
            (fs::u8path(projectRoot) / "Intermediate" / "Transactions")
            .lexically_normal()
            .generic_u8string();

        ProjectDocumentTransaction transaction;
        const auto transactionResult = transaction.Execute(std::move(writes), options);
        if (!transactionResult.success)
        {
            error = "S6 adoption transaction failed [" + transactionResult.code +
                "]: " + transactionResult.message;
            return false;
        }

        result.projectSourcePath = ProjectPathFor(package.packageId, entryPath);
        result.binding = BuildBinding(package, *packageEntry, closure);
        auto metadata = EvaluateScriptMetadata(projectRoot, result.projectSourcePath);
        if (!metadata.succeeded)
        {
            error = "S6 adopted package entry no longer has valid creator metadata.";
            return false;
        }
        result.binding.presentation = metadata.descriptor.presentation;
        result.adopted = true;
        result.updated = !previousVersion.empty() &&
            previousVersion != package.packageVersion;
        result.noChanges = transactionResult.noChanges;
        error.clear();
        return true;
    }
}
