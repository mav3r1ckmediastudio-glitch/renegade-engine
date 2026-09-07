#pragma once

#include "renegade/bridge/ScriptDocumentService.h"
#include "renegade/bridge/ScriptMetadataService.h"

#include <iterator>
#include <string>
#include <vector>

namespace renegade::bridge
{
    inline constexpr const char* ScriptLibraryPackageFilename =
        "renegade-script-package.json";
    inline constexpr const char* ScriptLibraryPackageSchema =
        "renegade-script-package";
    inline constexpr std::uint32_t ScriptLibraryPackageSchemaVersion = 1;
    inline constexpr const char* ScriptLibraryLockRelativePath =
        "Content/Scripts/.renegade/library-lock.json";

    // One creator-facing entry from an immutable installed package. The source
    // path is already projected into the project-owned Content/Scripts namespace
    // even before first-use adoption, so attaching the entry never creates a
    // second Runtime path vocabulary.
    struct ScriptLibraryEntry
    {
        std::string manifestPath;
        std::string packageRoot;
        std::string packageId;
        std::string packageVersion;
        std::string packageName;
        std::string entryPath;
        std::string projectSourcePath;
        ScriptMetadataDescriptor metadata;
        ScriptSourceBinding binding;
        std::vector<std::string> projectClosurePaths;
        bool adopted = false;
        bool updateAvailable = false;
        bool localConflict = false;
    };

    struct ScriptLibraryAdoptionResult
    {
        std::string projectSourcePath;
        ScriptSourceBinding binding;
        std::vector<std::string> projectClosurePaths;
        bool adopted = false;
        bool updated = false;
        bool noChanges = false;
    };

    // S6A-S6F UI-free authority for installed script packages. It validates
    // immutable manifests/hashes, computes deterministic transitive closure,
    // copies the complete closure into project-owned Content/Scripts through
    // ProjectDocumentTransaction, and records an adoption baseline used to
    // block destructive updates when a creator has edited the local copy.
    class ScriptLibraryService final
    {
    public:
        [[nodiscard]] static std::string DefaultInstalledLibraryRoot();

        [[nodiscard]] bool EnumerateEntries(
            const std::string& projectRoot,
            ScriptPresentation presentation,
            std::vector<ScriptLibraryEntry>& entries,
            std::vector<ScriptMetadataDiagnostic>& diagnostics,
            std::string& error,
            const std::string& installedLibraryRoot = {}) const;

        [[nodiscard]] bool AdoptEntry(
            const std::string& projectRoot,
            const std::string& manifestPath,
            const std::string& entryPath,
            ScriptLibraryAdoptionResult& result,
            std::string& error) const;
    };
}
